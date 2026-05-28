# duckdns

## What is this?

**duckdns** is the [DuckDNS](https://www.duckdns.org) dynamic-DNS
client for [spangap](../spangap) devices. It posts the device's current
public IP to DuckDNS, and provides the DNS-01 TXT-record path that
[acme](../acme) uses to obtain a TLS certificate.

## What this straddle owns

```
duckdns/
└── esp-idf/
    ├── include/duckdns.h
    └── src/duckdns.cpp
```

Plus a browser settings panel (under `browser/`) for token + domain.

## How others use it

```cpp
duckdnsInit();    // after netInit
```

Configuration:

- `s.duckdns.enable` — on/off
- `s.duckdns.domain` — your subdomain (`mything` for `mything.duckdns.org`)
- `secrets.duckdns.token` — your DuckDNS token, never sent to the
  browser

A cron entry refreshes A/AAAA periodically (and on `NET_EV_UPSTREAM_UP`
edges). `acmeInit()` discovers DuckDNS via the TXT-capable-provider
registry and uses it for DNS-01.

## Dependencies

- [spangap-net](../spangap-net) — HTTPS client to talk to DuckDNS.

## Read next

- [INTERNALS.md](INTERNALS.md) — request format, cadence, error
  handling.
- Cross-cutting remote-access doc:
  [spangap-core/docs/remote-access.md](../spangap-core/docs/remote-access.md).
