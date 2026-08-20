# duckdns — internals

Maintainer reference for the DuckDNS client. The [README](README.md) is the
operator guide; this document is for changing the code without breaking it.

## 1. What this straddle adds

Everything here is original — there is no upstream library. The whole module is
`esp-idf/src/duckdns.cpp` (public API in `esp-idf/include/duckdns.h`):

- A net-event-driven A-record updater (`s.duckdns.domain` + `s.duckdns.token`).
- A `dns.txtrecord` storage subscriber that publishes/clears the subdomain's TXT
  record for `acme`'s DNS-01 challenge.
- The `dns.txtrecord.capable` capability advertisement.
- A cron entry (`duckdns update`, every 15 minutes) and two CLI verbs
  (`duckdns`, `duckdns update`).

It owns no ITS ports and exposes no socket API — storage and CLI are the entire
control surface. The web/LCD settings pane is **generated** from the `settings:`
block in `straddle.yaml`; `browser/` ships only a `package.json`, no source.

## 2. The HTTP request shapes

Every call is a single HTTPS GET to `https://www.duckdns.org/update` with
`verbose=true`, a 10 s timeout, and the ESP cert bundle. A response is a success
only when the status is 200 and the body contains `OK`.

**A-record update** (`duckdnsUpdateTask`):

```
?domains=<sub>&token=<token>&ip=<ip>&verbose=true     (ip from upnpExternalIp())
?domains=<sub>&token=<token>&verbose=true             (ip omitted → DuckDNS auto-detects)
```

`ip` is sent only when `CONFIG_SPANGAP_UPNP` is defined *and* `upnpExternalIp()`
returns a non-empty address. There is no AAAA / IPv6 path — A record only.

**TXT record** (`duckdnsTxtTask`), driven by the `dns.txtrecord` subscription:

```
?domains=<sub>&token=<token>&txt=<value>&verbose=true       (set)
?domains=<sub>&token=<token>&txt=&clear=true&verbose=true    (clear, when value is empty)
```

The `<value>` is whatever `acme` wrote to `dns.txtrecord` — the challenge string,
not the DuckDNS token.

## 3. Task model

HTTPS needs a real stack, so each network call runs on its own temporary task
spawned via `spawnTask(..., 8192, ..., prio 1, core 0)`, which `killSelf()`s on
completion:

- `duckdnsUpdateTask` (task name `duckdns`) — the A-record update. Guarded by a
  `volatile bool updateBusy` so `duckdnsUpdate()` collapses concurrent requests
  into one in-flight update; the cron tick and a manual `duckdns update` can't
  pile up. It calls `netActivity()` to register network activity for power
  management.
- `duckdnsTxtTask` (task name `ddtxt`) — one per `dns.txtrecord` change, with the
  new value `strdup`'d and handed to the task (which `free`s it).

`lastIp[48]` and `lastStatus[8]` are plain RAM globals updated by the update task
and read by `duckdnsStatus`; `duckdnsStop` (on network-down) clears them.

## 4. Lifecycle

`duckdnsInit()` (called by the build's generated init) wires everything:

- `netRegister(NET_EV_UPSTREAM_UP, duckdnsStart)` — spawns the initial update
  task when the network comes up (no-op if unconfigured).
- `netRegister(NET_EV_UPSTREAM_DOWN, duckdnsStop)` — clears the cached state.
- `netRegister(NET_EV_UPSTREAM_UP, duckdnsNetCfg)` — subscribes to
  `dns.txtrecord` (see the pitfall below).
- Sets `dns.txtrecord.capable = 1` if a domain and token are configured.
- Registers the `duckdns` and `duckdns update` CLI commands.
- Keeps `s.cron.tab.duckdns = "*/15 * * * * N duckdns update"` in step with the
  configuration (`duckdnsApplyCron`): present while a domain and token are set
  (`storageDefault`, so a schedule tweak survives), removed otherwise. Applied
  at init and via a storage-task-hosted subscription on `s.duckdns`.

## 5. Pitfalls

- **Subscribe to `dns.txtrecord` from a net-event callback, not at init.** The
  subscription is registered in `duckdnsNetCfg` (a `NET_EV_UPSTREAM_UP`
  handler) rather than directly in `duckdnsInit`, because `app_main` deletes
  itself after boot — a `storageSubscribeChanges` registered on that task would
  never fire once the task is gone. A `static bool subscribed` makes it one-shot
  across repeated network-up edges.
- **Gate the `upnp.h` include after the spangap headers.** The `#if
  CONFIG_SPANGAP_UPNP` must come after the includes that transitively pull in
  `sdkconfig.h`; gating before that symbol is in scope silently skips the UPnP
  integration even when upnp is staged.
- **The TXT value is the challenge string, not the token.** Don't confuse the
  `token=` parameter (auth) with `txt=` (the value being published). They are
  unrelated.
