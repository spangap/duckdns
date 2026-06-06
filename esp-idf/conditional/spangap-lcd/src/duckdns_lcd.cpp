/**
 * duckdns_lcd.cpp — on-device Settings → Net → DuckDNS pane (mirrors the
 * browser DuckDnsPanel).
 *
 * This whole file lives under conditional/spangap-lcd/, compiled only when the
 * spangap-lcd straddle is staged, so no #if guard is needed. It registers via
 * the when:-gated duckdnsLcdRegister init hook rather than from duckdnsInit.
 */
#include "lcd.h"

/* On-device Settings → Net → DuckDNS pane. Mirrors the browser DuckDnsPanel. */
static void duckdnsSettingsPane(void* arg) {
    lv_obj_t* p = static_cast<lv_obj_t*>(arg);
    lcdSettingSection(p, "DuckDNS");
    lcdSettingText   (p, "Subdomain", "s.duckdns.domain");
    lcdSettingText   (p, "Token",     "s.duckdns.token", true);   /* secret */
}

/* Register the DuckDNS settings pane — a when:-gated init: hook
 * (spangap/spangap-lcd). Plain C++ linkage to match the generated
 * dispatcher's forward decl. */
void duckdnsLcdRegister(void) {
    lcdRegisterSettings("Net/DuckDNS", "DuckDNS", duckdnsSettingsPane);
}
