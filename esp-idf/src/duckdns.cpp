/**
 * DuckDNS dynamic DNS client.
 *
 * Updates A record with external IP, supports TXT records for ACME DNS-01.
 * HTTPS calls run on temp tasks (8KB stack). Periodic updates scheduled via
 * crontab ("duckdns update") — this module only does an initial update on
 * network up and reacts to UPnP external-IP changes.
 */
#include "duckdns.h"
#include "storage.h"
#include "cron.h"
#include "cli.h"
#include "log.h"
// upnp is an optional integration — gate the include AFTER the spangap headers
// above (which transitively pull in sdkconfig.h), so CONFIG_SPANGAP_UPNP is
// actually defined when this #if is evaluated. (Same ordering acme uses for
// CONFIG_SPANGAP_WEB — gating before sdkconfig.h is in scope silently skips it.)
#if CONFIG_SPANGAP_UPNP
#include "upnp.h"
#endif
#include "compat.h"
#include "net.h"
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include "esp_heap_caps.h"
#include <string>
#include <cstring>
#include <cstdio>

/* Module config version. Bump when adding/changing defaults that need
 * installing on existing devices (storageDefault is set-if-absent, so new
 * keys are picked up automatically; cronDefault is gated by the version
 * bump so user edits to the entry are preserved across reboots). */
#define DUCKDNS_VERSION 1

/* ---- State ---- */

static volatile bool updateBusy = false;
static char lastIp[48] = {};
static char lastStatus[8] = {};

/* ---- HTTP helper ---- */

struct http_ctx_t { std::string body; };

static esp_err_t httpEvent(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        auto* ctx = (http_ctx_t*)evt->user_data;
        if (ctx) ctx->body.append((const char*)evt->data, evt->data_len);
    }
    return ESP_OK;
}

static bool duckdnsGet(const char* url) {
    http_ctx_t ctx;
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = httpEvent;
    config.user_data = &ctx;
    config.timeout_ms = 10000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    auto client = esp_http_client_init(&config);
    esp_err_t e = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (e != ESP_OK) {
        info("HTTP error: %s\n", esp_err_to_name(e));
        snprintf(lastStatus, sizeof(lastStatus), "err");
        return false;
    }

    dbg("HTTP %d, body: %.*s\n", status, (int)std::min(ctx.body.size(), (size_t)80), ctx.body.c_str());

    if (status != 200) {
        info("HTTP %d\n", status);
        snprintf(lastStatus, sizeof(lastStatus), "%d", status);
        return false;
    }

    bool ok = ctx.body.find("OK") != std::string::npos;
    snprintf(lastStatus, sizeof(lastStatus), "%s", ok ? "OK" : "FAIL");
    return ok;
}

/* ---- Config check ---- */

static bool configured(char* domain, size_t domainLen, char* token, size_t tokenLen) {
    storageGetStr("s.duckdns.domain", domain, domainLen);
    storageGetStr("s.duckdns.token", token, tokenLen);
    return domain[0] && token[0];
}

/* ---- Update task ---- */

static void duckdnsUpdateTask(void*) {
    updateBusy = true;
    netActivity();
    char domain[64], token[80];
    if (!configured(domain, sizeof(domain), token, sizeof(token))) {
        updateBusy = false;
        killSelf();
        return;
    }

    /* Use external IP from UPnP if available, otherwise let DuckDNS auto-detect.
     * UPnP is an optional integration: without it `ip` stays empty and DuckDNS
     * auto-detects the public IP from the request source (the else branch below). */
#if CONFIG_SPANGAP_UPNP
    const char* ip = upnpExternalIp();
#else
    const char* ip = "";
#endif
    char url[256];
    if (ip[0])
        snprintf(url, sizeof(url),
            "https://www.duckdns.org/update?domains=%s&token=%s&ip=%s&verbose=true",
            domain, token, ip);
    else
        snprintf(url, sizeof(url),
            "https://www.duckdns.org/update?domains=%s&token=%s&verbose=true",
            domain, token);

    if (duckdnsGet(url)) {
        snprintf(lastIp, sizeof(lastIp), "%s", ip[0] ? ip : "(auto)");
        info("%s.duckdns.org → %s\n", domain, lastIp);
    } else {
        err("DuckDNS update failed\n");
    }

    updateBusy = false;
    killSelf();
}

/* ---- TXT record management (for ACME DNS-01) ---- */

static void duckdnsTxtTask(void* arg) {
    char* txt = (char*)arg;
    char domain[64], token[80];
    if (configured(domain, sizeof(domain), token, sizeof(token))) {
        char url[512];
        if (txt[0]) {
            snprintf(url, sizeof(url),
                "https://www.duckdns.org/update?domains=%s&token=%s&txt=%s&verbose=true",
                domain, token, txt);
            bool ok = duckdnsGet(url);
            info("DuckDNS TXT %s: %s\n", ok ? "set" : "failed", txt);
        } else {
            snprintf(url, sizeof(url),
                "https://www.duckdns.org/update?domains=%s&token=%s&txt=&clear=true&verbose=true",
                domain, token);
            duckdnsGet(url);
        }
    }
    free(txt);
    killSelf();
}

static void duckdnsOnTxtChange(const char* key, const char* val) {
    /* Spawn temp task — HTTPS needs stack */
    char* txt = strdup(val);
    if (!txt) return;
    spawnTask(duckdnsTxtTask, "ddtxt", 8192, txt, 1, 0);
}

/* ---- Public API ---- */

static void duckdnsStart(const char*) {
    char domain[64], token[80];
    if (!configured(domain, sizeof(domain), token, sizeof(token))) return;

    /* Initial update on net-up. Further updates scheduled via crontab. */
    spawnTask(duckdnsUpdateTask, "duckdns", 8192, nullptr, 1, 0);
}

static void duckdnsStop(const char*) {
    lastIp[0] = '\0';
    lastStatus[0] = '\0';
}

void duckdnsUpdate() {
    if (updateBusy) return;
    spawnTask(duckdnsUpdateTask, "duckdns", 8192, nullptr, 1, 0);
}

static void duckdnsStatus(cli_write_fn write) {
    char buf[128];
    int n;
    char domain[64], token[80];
    if (!configured(domain, sizeof(domain), token, sizeof(token))) {
        n = snprintf(buf, sizeof(buf), "duckdns: not configured\n");
        write(buf, (size_t)n);
        return;
    }
    n = snprintf(buf, sizeof(buf), "domain: %s.duckdns.org\n", domain);
    write(buf, (size_t)n);
    if (lastIp[0]) {
        n = snprintf(buf, sizeof(buf), "ip: %s\n", lastIp);
        write(buf, (size_t)n);
    }
    if (lastStatus[0]) {
        n = snprintf(buf, sizeof(buf), "status: %s\n", lastStatus);
        write(buf, (size_t)n);
    }
}

static void duckdnsNetCfg(const char*) {
    /* Subscribe on net task (stays alive) — app_main deletes itself after boot,
     * so subscriptions registered there never fire. One-shot: only subscribe once. */
    static bool subscribed = false;
    if (!subscribed) {
        storageSubscribeChanges("dns.txtrecord", duckdnsOnTxtChange);
        subscribed = true;
    }
}

void duckdnsInit() {
    /* Self-register: install own defaults + cron entry on first run / upgrade. */
    int v = storageGetInt("s.duckdns.version", 0);
    if (v < DUCKDNS_VERSION) {
        /* s.duckdns.{domain,token} defaults are seeded by the generated
         * spangapSettingsGenDefaults() from this straddle's `settings:` block. */
        cronDefault("*/15 * * * * N", "duckdns update");
        storageSet("s.duckdns.version", DUCKDNS_VERSION);
    }

    netRegister(NET_EV_UPSTREAM_UP,   duckdnsStart);
    netRegister(NET_EV_UPSTREAM_DOWN, duckdnsStop);
    netRegister(NET_EV_UPSTREAM_UP,   duckdnsNetCfg);
    /* Advertise TXT record capability if configured */
    char domain[64], token[80];
    if (configured(domain, sizeof(domain), token, sizeof(token)))
        storageSet("dns.txtrecord.capable", 1);
    static auto w = [](const char* d, size_t l) { cliPrintf("%.*s", (int)l, d); };
    cliRegisterCmd("duckdns update", [](const char* a) {
        if (cliWantsHelp(a)) { cliPrintf("%-*s force DNS update\n", CLI_HELP_COL, "duckdns update"); return; }
        duckdnsUpdate();
    });
    cliRegisterCmd("duckdns", [](const char* a) {
        if (cliWantsHelp(a)) { cliPrintf("%-*s DuckDNS status\n", CLI_HELP_COL, "duckdns"); return; }
        duckdnsStatus(w);
    });
}
