/**
 * DuckDNS dynamic DNS client — updates A record, sets TXT for ACME.
 * All HTTPS operations async on temp tasks (except blocking TXT for ACME).
 */
#ifndef SPANGAP_DUCKDNS_H
#define SPANGAP_DUCKDNS_H

#include "service.h"

/** The DuckDNS service. onInit registers DuckDNS net event callbacks + CLI +
 *  dns.txtrecord subscriber at boot (after netInit). */
class DuckdnsService : public Service {
public:
    void onInit() override;
};

/** Force an immediate DNS update (spawns temp task). */
void duckdnsUpdate();

#endif
