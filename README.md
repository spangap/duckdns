# duckdns — DuckDNS dynamic DNS client

**duckdns** keeps a stable public hostname pointed at the device's changing
public IP using [DuckDNS](https://www.duckdns.org), and publishes the DNS TXT
record that [acme](../acme) needs for a DNS-01 certificate challenge. It updates
a single **A record** (IPv4 only) for one DuckDNS subdomain.

## What it does

When the network comes up, duckdns does one A-record update, then a cron entry
(`duckdns update`, every 15 minutes) keeps it fresh. If [upnp](../upnp) is in the
build and has learned the gateway's external IP, that IP is sent explicitly;
otherwise the `ip` parameter is omitted and DuckDNS infers the public IP from the
request's source address.

Enablement is **implicit**: there is no enable switch. With no subdomain and no
token set, every entry point is a no-op — nothing is sent. Set both and the
straddle becomes active on the next network-up edge (or on the next cron tick).

It starts automatically when the straddle is in the build; there is no init call
to make.

### How it interacts with other straddles

- **[acme](../acme) — DNS-01 challenge.** The hand-off is through ephemeral
  `dns.*` storage vars, not a direct call. At startup, if duckdns is configured,
  it sets `dns.txtrecord.capable = 1` so `acme` can auto-select DNS-01. During a
  challenge, `acme` writes the challenge value to `dns.txtrecord`; duckdns
  **subscribes** to that key and publishes it as the subdomain's TXT record on
  change (writing an empty value clears the record). The TXT value is the ACME
  challenge string — it is **not** the DuckDNS token. The cross-cutting overview
  is in [spangap-core/docs/remote-access.md](../spangap-core/docs/remote-access.md).
- **[upnp](../upnp) — external IP (optional).** Used only as a source for the
  explicit `ip=` parameter. duckdns compiles against it only when it is staged;
  without it, DuckDNS auto-detection covers the same need.
- **[spangap-net](../spangap-net)** — provides the HTTPS client (and the
  network-up/down events that drive the initial update).

## Storage variables

### Settings (read)

| Key | Default | Meaning |
|---|---|---|
| `s.duckdns.domain` | `""` | The subdomain only — `myname` for `myname.duckdns.org`. |
| `s.duckdns.token` | `""` | DuckDNS account token. **Secret**: masked on the LCD, write-only on the web (never read back to the browser). |

These two keys are owned here, but their defaults are declared in this straddle's
`straddle.yaml` `settings:` block (which also generates the LCD pane and the web
panel) — they are not set in code. See [INTERNALS.md](INTERNALS.md).

### Integration vars (written / subscribed)

| Key | Direction | Meaning |
|---|---|---|
| `dns.txtrecord.capable` | written `1` at startup | Advertises that a provider can serve DNS TXT records, when duckdns is configured. `acme` reads it to pick DNS-01. |
| `dns.txtrecord` | subscribed | The ACME challenge value; `acme` writes it, duckdns publishes it as the TXT record (empty value clears it). |

### Runtime state (in RAM, not storage)

`duckdns` (status, below) reports the last update's IP and status, held in RAM
only — they are not published to storage and reset on reboot or on network-down.

## CLI

```
duckdns            DuckDNS status — domain, last IP, last result
duckdns update     force an immediate A-record update
```

`duckdns` with nothing configured prints `duckdns: not configured`. Otherwise it
prints the full hostname, the IP used by the last update (an explicit address, or
`(auto)` when DuckDNS detected it), and the last result (`OK`, `FAIL`, an HTTP
status code, or `err`). Run either on-device through `spangap cli "<command>"`.

## Dependencies

- [spangap-net](../spangap-net) — HTTPS client and network events.
- [upnp](../upnp) — optional; external-IP source only.

## Read next

- [INTERNALS.md](INTERNALS.md) — request shapes, the temp-task model, the
  `dns.txtrecord` subscription contract, and maintainer pitfalls.
- [spangap-core/docs/remote-access.md](../spangap-core/docs/remote-access.md) —
  how upnp / duckdns / acme fit together.
