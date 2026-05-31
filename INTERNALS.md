# duckdns — internals

## Request shape

DuckDNS API is a simple GET to:

```
https://www.duckdns.org/update?domains=<sub>&token=<token>&ip=<ip>&txt=<txt>
```

`ip` is empty by default (DuckDNS infers from the request source IP).
For DNS-01 the request also sets `txt=<token>`.

## Cadence

A cron entry runs the A/AAAA refresh every 5–15 minutes when upstream
is healthy. `netRegister(NET_EV_UPSTREAM_UP, …)` triggers a refresh on
re-connect.

## TXT for DNS-01

`duckdnsSetTxt(value)` / `duckdnsClearTxt()` — called by `acme` during
a DNS-01 challenge. After clearing, DuckDNS removes the record on the
next refresh.

## Why this is its own straddle

It's a tiny piece of code, but it's a *transitive* dependency for the
DNS-01 challenge path in `acme`. Keeping it separate means
`spangap/acme` can declare `requires: spangap/duckdns` without `acme`
itself carrying DuckDNS-specific code. A future Cloudflare- or
Route53-backed straddle would slot in the same way.

## Token handling

The token lives under `secrets.duckdns.token` — never sent to the
browser; it is set via the operator panel (POST-only to the device) and
displayed back only as a masked indicator.
