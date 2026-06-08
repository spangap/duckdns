/**
 * DuckDNS dynamic DNS client — updates A record, sets TXT for ACME.
 * All HTTPS operations async on temp tasks (except blocking TXT for ACME).
 */
#ifndef SPANGAP_DUCKDNS_H
#define SPANGAP_DUCKDNS_H

/** Register DuckDNS net event callbacks + CLI + dns.txtrecord subscriber.
 *  Call from main after netInit(). */
void duckdnsInit();

/** Force an immediate DNS update (spawns temp task). */
void duckdnsUpdate();

#endif
