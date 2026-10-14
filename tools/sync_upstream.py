#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate the Arduino library sources (src/) from the netbird-lite ESP-IDF component.

Usage:
  tools/sync_upstream.py UPSTREAM_DIR            regenerate src/ from a netbird-esp32 checkout
  tools/sync_upstream.py UPSTREAM_DIR --check    exit 1 if src/ differs from what would be generated

UPSTREAM_DIR is a checkout of https://github.com/insight-it-gmbh/netbird-esp32. The commit it is
at is recorded in the UPSTREAM file. Everything below src/ except the hand-written files listed in
HANDWRITTEN is generated: never edit it by hand, change the component upstream and regenerate.

What the generator does, and why:
- Arduino compiles every source file below src/, but only puts src/ itself on the include path.
  The ESP-IDF build uses several private include directories instead (see the component's
  CMakeLists.txt). Every #include that resolved through one of those directories is rewritten to
  a path relative to the including file, so that the result compiles with Arduino's single
  include directory. Vendored headers stay in subdirectories and cannot shadow other libraries.
- Arduino has no per-library Kconfig and no per-library compiler flags. Every generated .c file
  therefore starts with #include "nbl_arduino_config.h", which supplies the Kconfig defaults, the
  compile definitions of the CMake build and the symbol prefixes below.
- Vendored code exports generic symbols (pb_decode, x25519, wireguardif_init, ...). Other
  Arduino libraries bundle the same code, most notably wireguard-lwip, and the linker would then
  fail with duplicate definitions. tools/vendor_symbols.txt lists those symbols; the generated
  src/nbl_arduino_symbols.h maps each of them to an nblx_ name.
- The vendored files get the same warning suppressions as in the ESP-IDF build, as pragmas,
  because the Arduino IDE's "All" warning level turns warnings into errors.
- Only the HTTP/2 implementation built into netbird-lite is copied; the nghttp2 backend is not
  available in the Arduino core.
"""
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMPONENT = "components/netbird_lite"
# hand-written files in src/ that the generator must keep
HANDWRITTEN = {"NetBirdLite.h", "NetBirdLite.cpp", "nbl_arduino_config.h"}
# component-relative directory -> directory below src/
LAYOUT = [("include", ""), ("src", ""), ("proto_gen", "proto_gen"), ("vendor", "vendor")]
# files copied besides sources and headers (licences and provenance of vendored code)
EXTRA_FILES = re.compile(r"(LICENSE|LICENCE|COPYING|README)[^/]*$|-license\.txt$|NBL-PATCH\.diff$", re.I)
INCLUDE_RE = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)([>"])', re.M)
WARNING_PRAGMAS = [
    "-Wunused-function", "-Wunused-variable", "-Wunused-but-set-variable", "-Wformat",
    "-Wsign-compare", "-Wstringop-overread", "-Warray-parameter", "-Warray-bounds",
]


def die(msg):
    sys.exit("sync_upstream: " + msg)


def parse_cmake(path):
    """Returns (sources, include_dirs) of the component, component-relative, as in the ESP-IDF build."""
    text = open(path, encoding="utf-8").read()

    def block(name):
        m = re.search(name + r"\s+(.*?)\n\s*(?:[A-Z_]+\s*$|\))", text, re.S | re.M)
        if not m:
            die("cannot find %s in %s" % (name, path))
        return re.findall(r'"([^"]+)"', m.group(1))

    srcs = block(r"\bSRCS")
    # the else-branch of the HTTP/2 choice: the built-in client
    if "src/nbl_h2_lite.c" not in text:
        die("src/nbl_h2_lite.c not referenced in CMakeLists.txt")
    srcs.append("src/nbl_h2_lite.c")
    incs = block(r"\bINCLUDE_DIRS") + block(r"\bPRIV_INCLUDE_DIRS")
    return srcs, incs


def map_path(rel):
    """component-relative path -> path below src/"""
    for top, dst in LAYOUT:
        if rel == top or rel.startswith(top + "/"):
            rest = rel[len(top):].lstrip("/")
            return os.path.normpath(os.path.join(dst, rest)) if rest else (dst or ".")
    die("no layout rule for " + rel)


def check_config_coverage(comp):
    """Every Kconfig option and compile definition of the component must be handled in
    src/nbl_arduino_config.h, otherwise an upstream change would silently fall back to whatever
    the code does without it."""
    cfg = open(os.path.join(ROOT, "src", "nbl_arduino_config.h"), encoding="utf-8").read()
    kconfig = open(os.path.join(comp, "Kconfig"), encoding="utf-8").read()
    names = ["CONFIG_" + n for n in re.findall(r"^\s*config\s+(\w+)", kconfig, re.M)]
    cmake = open(os.path.join(comp, "CMakeLists.txt"), encoding="utf-8").read()
    m = re.search(r"target_compile_definitions\([^)]*?PRIVATE(.*?)\)", cmake, re.S)
    if m:
        names += re.findall(r"^\s*(\w+)=", m.group(1), re.M)
    missing = [n for n in names if not re.search(r"\b%s\b" % re.escape(n), cfg)]
    if missing:
        die("not handled in src/nbl_arduino_config.h: " + ", ".join(missing))


def generate(upstream, out):
    comp = os.path.join(upstream, COMPONENT)
    check_config_coverage(comp)
    srcs, incs = parse_cmake(os.path.join(comp, "CMakeLists.txt"))
    files = set(srcs)
    for top, _ in LAYOUT:
        for dirpath, _, names in os.walk(os.path.join(comp, top)):
            for n in names:
                rel = os.path.relpath(os.path.join(dirpath, n), comp)
                if n.endswith((".h", ".inc")) or EXTRA_FILES.search(rel):
                    files.add(rel)
    for rel in srcs:
        if not os.path.isfile(os.path.join(comp, rel)):
            die("listed source missing upstream: " + rel)

    os.makedirs(out, exist_ok=True)
    targets = {}  # component-relative -> path below src/
    for rel in sorted(files):
        dst = map_path(rel)
        if dst in targets.values():
            die("two upstream files map to src/" + dst)
        targets[rel] = dst
    by_target = {v: k for k, v in targets.items()}
    inc_dirs = [map_path(d) for d in incs]

    def resolve(including, name, quoted):
        """Emulates the compiler's search in the ESP-IDF build. Returns a src/-relative path or None."""
        cands = [os.path.normpath(os.path.join(os.path.dirname(including), name))] if quoted else []
        cands += [os.path.normpath(os.path.join(d, name)) for d in inc_dirs]
        for c in cands:
            if c in by_target:
                return c
        return None

    vendored_symbols = []
    for rel, dst in sorted(targets.items()):
        src_path = os.path.join(comp, rel)
        dst_path = os.path.join(out, dst)
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        if not dst.endswith((".c", ".h", ".inc")):
            shutil.copyfile(src_path, dst_path)
            continue
        text = open(src_path, encoding="utf-8", errors="strict").read()

        def fix(m):
            name, quoted = m.group(3), m.group(2) == '"'
            hit = resolve(dst, name, quoted)
            if hit is None:
                return m.group(0)  # ESP-IDF / system header
            relpath = os.path.relpath(hit, os.path.dirname(dst) or ".")
            return '%s"%s"' % (m.group(1), relpath.replace(os.sep, "/"))

        text = INCLUDE_RE.sub(fix, text)
        if dst.endswith(".c"):
            head = ["/* Generated from netbird-esp32 %s by tools/sync_upstream.py - do not edit. */" % rel,
                    '#include "nbl_arduino_config.h"']
            if rel.startswith("vendor/"):
                head += ['#pragma GCC diagnostic ignored "%s"' % w for w in WARNING_PRAGMAS]
            text = "\n".join(head) + "\n" + text
        with open(dst_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)

    syms = [s.strip() for s in open(os.path.join(ROOT, "tools", "vendor_symbols.txt"), encoding="utf-8")
            if s.strip() and not s.startswith("#")]
    with open(os.path.join(out, "nbl_arduino_symbols.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/sync_upstream.py from tools/vendor_symbols.txt - do not edit.\n"
                " * Gives the global symbols of the vendored code an nblx_ prefix, so that other Arduino\n"
                " * libraries bundling the same code (nanopb, wireguard-lwip, TweetNaCl) link next to it. */\n"
                "#pragma once\n")
        for s in syms:
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", s):
                die("bad symbol name: " + s)
            f.write("#define %s nblx_%s\n" % (s, s))
    return len(targets)


def tree_files(d):
    out = set()
    for dirpath, _, names in os.walk(d):
        for n in names:
            out.add(os.path.relpath(os.path.join(dirpath, n), d))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check = "--check" in sys.argv[1:]
    if len(args) != 1 or any(a not in ("--check",) for a in sys.argv[1:] if a.startswith("--")):
        print(__doc__.strip().split("\n\n")[0])
        sys.exit(2)
    upstream = os.path.abspath(args[0])
    if not os.path.isdir(os.path.join(upstream, COMPONENT)):
        die("%s is not a netbird-esp32 checkout" % upstream)
    src = os.path.join(ROOT, "src")
    with tempfile.TemporaryDirectory() as tmp:
        n = generate(upstream, tmp)
        for h in HANDWRITTEN:
            if not os.path.isfile(os.path.join(src, h)):
                die("hand-written file missing: src/" + h)
        want = tree_files(tmp)
        have = tree_files(src) - HANDWRITTEN
        if check:
            diff = sorted((want ^ have) | {f for f in want & have
                                           if not filecmp.cmp(os.path.join(tmp, f), os.path.join(src, f), shallow=False)})
            if diff:
                print("src/ is not in sync with %s:" % upstream)
                for f in diff:
                    print("  " + f)
                sys.exit(1)
            print("src/ is in sync (%d files)" % n)
            return
        for f in have - want:
            os.remove(os.path.join(src, f))
        for f in want:
            os.makedirs(os.path.dirname(os.path.join(src, f)), exist_ok=True)
            shutil.copyfile(os.path.join(tmp, f), os.path.join(src, f))
        for dirpath, dirs, names in sorted(os.walk(src), reverse=True):
            if dirpath != src and not os.listdir(dirpath):
                os.rmdir(dirpath)
    try:
        sha = subprocess.run(["git", "-C", upstream, "rev-parse", "HEAD"], capture_output=True, text=True,
                             check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        sha = "unknown"
    with open(os.path.join(ROOT, "UPSTREAM"), "w", encoding="utf-8") as f:
        f.write("https://github.com/insight-it-gmbh/netbird-esp32 %s\n" % sha)
    print("generated %d files from %s (%s)" % (n, upstream, sha[:12]))


if __name__ == "__main__":
    main()
