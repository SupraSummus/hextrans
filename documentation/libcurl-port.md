# HTTP via libcurl

Four HTTP call sites — pakset download (`gui/pakinstaller.cc`),
server announce (`world/simworld.cc`), server list retrieval
(`gui/server_frame.cc`) and external-IP query (`network/network.cc`)
— route through libcurl.  Migrated from a hand-rolled BSD-socket
client (~600 LoC, plain HTTP only, no redirects beyond
absolute-`Location:` followers, explicit-`Cannot handle https.`
fall-through) plus a per-platform fan-out in `pakset_downloader.cc`
for HTTPS (system curl on Linux, `URLDownloadToFile` / PowerShell on
Windows, in-house HTTP elsewhere).

## Layout

`network_file_transfer.cc` ships two parallel HTTP backends gated by
the `USE_CURL` compile-time define (CMake option
`SIMUTRANS_USE_CURL`, autoconf `USE_CURL=1`, ON by default when
libcurl is found).  The libcurl branch holds `network_http_get`,
`network_http_post`, `network_http_get_file` (signatures unchanged
from the legacy in-house implementations they replaced) plus
`network_curl_download_url(url, filename)` for callers that own a
full URL — currently only the pakset installer.

The legacy in-house socket code is preserved under `#else` for one
release cycle so a downstream packager can fall back via
`-DSIMUTRANS_USE_CURL=OFF` if a regression slips out; retirement is
tracked in `TODO.md` → "other".  `parse_http_url` (the URL-splitting
helper added when the e2e suite was wired up) is only reachable from
the legacy branch now and retires with it.

`pakset_downloader.cc` collapses its desktop fan-out into a single
`network_curl_download_url` call on the libcurl path.  Android keeps
the JNI path (`download_file` extern) under `#ifdef __ANDROID__`,
unchanged — NDK builds of libcurl exist but the JNI seam is already
working, so we don't churn it.

## Behaviour libcurl brings that the legacy code didn't

  - Redirects (incl. relative `Location:`, explicit-port, http→https
    upgrades) are followed up to a 10-hop cap.
  - HTTPS works without a per-platform branch.
  - Chunked transfer encoding works.
  - `http_proxy` / `HTTPS_PROXY` / `NO_PROXY` env vars are honoured
    (libcurl default — see "Resolved decisions").
  - On Linux/macOS the system trust store is used; no CA bundle is
    shipped.  Windows / vcpkg Schannel builds also use the system
    store.

Three pre-existing bugs in `network_http_get_file` retire on the
libcurl path as a side effect: the literal `\nLocation: http://`
matcher (broke relative redirects), the unconditional `:80` append
that mangles explicit-port redirect targets (also bit
`pakset_downloader.cc`'s old URL split), and the mis-tagged "Cannot
handle https: Server returned %d" string returned for every non-200.
They remain present in the legacy `#else` branch since fixing them
there is wasted work ahead of retirement.

## Tests

`tools/http_fixture/` runs an in-tree HTTP fixture against a
headless simutrans pointed at it via the real production
`-listserver HOST[:PORT]` and `-ip_query_host HOST[:PORT]` flags
(backed by `env_t::listserver` / `env_t::ip_query_host`).  Two
tests:

  - `test_announce` covers `network_http_post` via
    `karte_t::announce_server`.
  - `test_external_ip` covers `network_http_get` via
    `get_external_IP`.

CI runs them under the existing clang+ASAN+UBSAN job.
`network_http_get_file` is GUI-only (pakset installer) and stays
uncovered until an honest production CLI seam exists — a test-only
flag would re-introduce the "fixtures in production code" pattern
the suite was reshaped to avoid.

## Resolved decisions

*Proxy policy.*  Take libcurl's default — respect `http_proxy` /
`HTTPS_PROXY` / `NO_PROXY`.  Zero lines of code, matches corporate
user expectations, and the current "ignore proxies" behaviour falls
out of using raw sockets rather than being a documented contract.

*Address family for external-IP detection.*  Take libcurl's default
— do not set `CURLOPT_IPRESOLVE`.  `get_external_IP` already handles
dual-stack at the *server* side: it calls `QUERY_ADDR_IP` (dual-stack
DNS record) and, when the response contains a colon (v6), follows up
with `QUERY_ADDR_IPv4_ONLY` (A-only record) to also get the v4
address.  Forcing `CURL_IPRESOLVE_V4` would break the query on
v6-only networks.

*Gate for one release cycle.*  Land behind `SIMUTRANS_USE_CURL`
(ON).  External network behaviour is impossible to fully verify in
CI, and the rollback path has to be cheaper than a revert.  This
contradicts the no-feature-flag norm elsewhere in the port; the
justification is the impossibility of full pre-merge verification,
not a habit to repeat.

## Open items

CA bundle on Windows.  vcpkg's curl builds against Schannel by
default which uses the Windows cert store; if a packager builds
against OpenSSL instead they need a bundle.  Document the supported
configuration; don't ship a bundle ourselves.

Android.  Kept on JNI under `#ifdef __ANDROID__`.  If someone wants
to consolidate later, NDK builds of libcurl exist but the JNI path
is already working — no reason to churn it now.

HTTP/2 negotiation.  Left at libcurl's default (ALPN-negotiated).
If the listserver or a pakset mirror stalls on HTTP/2 we'd pin
`CURL_HTTP_VERSION_1_1`; no evidence yet that any does.
