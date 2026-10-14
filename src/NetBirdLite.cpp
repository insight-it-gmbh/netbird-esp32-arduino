// SPDX-License-Identifier: Apache-2.0
#include "NetBirdLite.h"

#include <stdio.h>
#include <string.h>

NetBirdLiteClass NetBirdLite;

bool NetBirdLiteClass::begin(const char *managementUrl, const char *setupKey, const char *hostname)
{
    nbl_config_t cfg = {};
    cfg.management_url = managementUrl;
    cfg.setup_key = (setupKey && setupKey[0]) ? setupKey : nullptr;
    cfg.hostname = (hostname && hostname[0]) ? hostname : nullptr;
    _startErr = nbl_start(&cfg);
    return _startErr == ESP_OK;
}

void NetBirdLiteClass::end()
{
    nbl_stop();
}

nbl_status_t NetBirdLiteClass::status() const
{
    nbl_status_t st;
    nbl_get_status(&st);
    return st;
}

bool NetBirdLiteClass::connected() const
{
    return status().state == NBL_SYNCED;
}

nbl_state_t NetBirdLiteClass::state() const
{
    return status().state;
}

const char *NetBirdLiteClass::stateName() const
{
    switch (state()) {
    case NBL_IDLE: return "IDLE";
    case NBL_WAIT_TIME: return "WAIT_TIME";
    case NBL_CONNECTING: return "CONNECTING";
    case NBL_LOGIN: return "LOGIN";
    case NBL_SYNCED: return "SYNCED";
    case NBL_RETRY_WAIT: return "RETRY_WAIT";
    case NBL_FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

/* st.ip is "a.b.c.d/p" or empty */
static bool parse_ip(const char *s, uint8_t a[4], unsigned *prefix)
{
    unsigned b[4], p = 32;
    char tail = 0;
    int n = sscanf(s, "%3u.%3u.%3u.%3u/%2u%c", &b[0], &b[1], &b[2], &b[3], &p, &tail);
    if (n < 4 || n > 5 || b[0] > 255 || b[1] > 255 || b[2] > 255 || b[3] > 255 || p > 32) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        a[i] = (uint8_t)b[i];
    }
    *prefix = p;
    return true;
}

IPAddress NetBirdLiteClass::localIP() const
{
    nbl_status_t st = status();
    uint8_t a[4];
    unsigned p;
    if (!parse_ip(st.ip, a, &p)) {
        return IPAddress((uint32_t)0);
    }
    return IPAddress(a[0], a[1], a[2], a[3]);
}

uint8_t NetBirdLiteClass::prefixLength() const
{
    nbl_status_t st = status();
    uint8_t a[4];
    unsigned p;
    return parse_ip(st.ip, a, &p) ? (uint8_t)p : 0;
}

String NetBirdLiteClass::publicKey() const
{
    return String(status().pubkey);
}

String NetBirdLiteClass::lastError() const
{
    return String(status().last_error);
}

int NetBirdLiteClass::peers() const
{
    return status().peers;
}

int NetBirdLiteClass::peersUp() const
{
    return status().peers_up;
}

bool NetBirdLiteClass::signalConnected() const
{
    return status().signal_up;
}

bool NetBirdLiteClass::relayConnected() const
{
    return status().relay_up;
}

void NetBirdLiteClass::forget()
{
    nbl_forget();
}
