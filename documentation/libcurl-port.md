# Replacing the in-house HTTP code with libcurl

Plan doc for swapping the hand-rolled HTTP client in
`src/simutrans/network/network_file_transfer.cc` and
`src/simutrans/dataobj/pakset_downloader.cc` for libcurl.  When the
work lands, this doc describes live behaviour; the open items move to
`TODO.md` and the rest gets pruned.

## Why

Four call sites use HTTP today: pakset downloads
(`gui/pakinstaller.cc`), server announce
(`world/simworld.cc:6698`), server list retrieval
(`gui/server_frame.cc:291`), and external IP detection
(`network/network.cc:926`).  The in-house code is ~600 LoC of raw
BSD-socket parsing with platform forks for winsock2.  It explicitly
rejects HTTPS — `"Cannot handle https."` at
`network_file_transfer.cc:581` — which already blocks pakset hosts
that have moved HTTPS-only (GitHub Releases, GitHub Pages, most
modern mirrors) and gets worse over time.  Integrity, not just
secrecy, is at stake: a MITM injecting a hostile pakset zip into a
plain-HTTP download is the textbook attack.

A previous libcurl attempt is in-tree as `#if 0` at
`pakset_downloader.cc:271-448`, broken and bit-rotted.  We're
re-doing it, not restoring it.

On Linux, simutrans already depends on curl at runtime: the HTTPS
pakset-download branch in `pakset_downloader.cc:220` shells out to
`/usr/bin/curl --progress-bar -L '<url>' > 'temp.zip'`.  The
codebase is therefore already split between in-house plain-HTTP
sockets, system curl for Linux HTTPS, `URLDownloadToFile` /
PowerShell on Windows HTTPS, and JNI on Android.  The libcurl port
unifies that fan-out behind a single backend on desktop platforms
(Android keeps JNI), so this is consolidation, not a new
dependency.

## In-house bugs to retire with the port

Three real bugs surfaced in `network_http_get_file` while building
the e2e baseline.  The libcurl wrapper is expected to fix each:

  - **Relative `Location:` redirects.**  The handler only matches
    `\nLocation: http://` literally; relative paths hit the
    `"Unknown redirect."` fall-through.  RFC 7231 / libcurl follow
    them.
  - **Explicit-port redirects.**  The handler unconditionally
    `strcpy(c, ":80")`s after splitting the redirect host, so a
    URL like `http://x:8080/p` round-trips as `x:8080:80` and
    `network_open_address` returns "Bad address".  Real
    listservers redirect without explicit ports, which is why this
    never bit in production.
  - **Mis-tagged error string.**  `network_http_get_file` returns
    `"Cannot handle https: Server returned %d"` for *every*
    non-200 response on plain HTTP — the message is wrong.

`pakset_downloader.cc` also had the same `:80` double-append bug
in its URL-splitting logic (line 236) and is now fixed via a
shared helper.  These three bugs aren't covered by the current e2e
suite — exercising `network_http_get_file` without a test-only CLI
hook needs an honest production seam (e.g. a pakset install from
the command line) that doesn't exist yet, and adding a test flag
would re-introduce exactly the "fixtures in production code"
pattern the suite was reshaped to avoid.  The libcurl migration
itself remains the trigger to fix them.

## The forum objection

prissi raised four points on the simutrans forum (topic 22608):
~100 MB for the CA chain, libcurl doesn't work on Android, no
security-sensitive data, each library is maintenance burden.  Three
are wrong on the facts — Mozilla's bundle is ~200 KB and on most
platforms we use the system trust store and ship nothing; libcurl
builds under the Android NDK and is widely deployed; integrity
matters even when secrecy doesn't.  The fourth is true in general but
libcurl is a system package on every target platform.  The Android
question is the only real friction and we handle it by keeping the
existing JNI download path under `#ifdef __ANDROID__` rather than
forcing one backend everywhere.

## Non-goals

Not changing the binary network protocol used between simutrans
clients and servers (the custom non-HTTP socket protocol in
`network.cc`).  Not adding new features — no resume-on-partial, no
parallel downloads, no auth.  Not changing the public C++ API at the
call sites if avoidable; ideally `network_http_get()` /
`network_http_post()` keep their signatures and just gain HTTPS.

## Scope

Files deleted or heavily reduced:

  - `src/simutrans/dataobj/pakset_downloader.cc` — the active code
    is mostly platform forks (urlmon on Windows, JNI on Android, raw
    HTTP elsewhere).  After the port: a thin file that on Android
    still routes through JNI, and on everything else delegates to
    the curl wrapper.

Files modified:

  - `src/simutrans/network/network_file_transfer.{h,cc}` — replace
    `network_http_get*()` and `network_http_post()` with libcurl
    calls.  Signatures preserved.  ~250 LoC of socket-parsing
    deleted, ~100 LoC of curl wrapper added.  `parse_http_url()`
    (added to support the e2e suite, reused by
    `pakset_downloader.cc`) is the URL-handling seam for the
    wrapper.

The e2e suite redirects HTTP traffic to a local fixture through two
real CLI flags — `-listserver HOST[:PORT]` and `-ip_query_host
HOST[:PORT]` — backed by `env_t::listserver` and
`env_t::ip_query_host` plus matching `listserver` / `ip_query_host`
keys in `simuconf.tab`.  These are shipped configuration knobs for
anyone running a private simutrans network or fork, not testing
infrastructure: empty defaults preserve the hardcoded
`ANNOUNCE_SERVER1` / `QUERY_ADDR_IP` lookups, and the libcurl
wrapper inherits both call sites unchanged.
  - `CMakeLists.txt` and `cmake/SimutransFindDependencies.cmake` —
    `find_package(CURL REQUIRED)` plus link, behind a CMake option
    for one release cycle so a downstream packager can fall back to
    the legacy code if a regression slips out.
  - `cmake/SimutransSourceList.cmake` — adjust if file layout
    changes.
  - `tools/session-start.sh` — add `libcurl4-openssl-dev` to the
    apt-get list.
  - `.github/workflows/run-tests.yml` — confirm libcurl is on the
    runner image (it is, by default); add a job that exercises the
    fixture suite below.

## Approach

Fixture-first.  Before any libcurl code lands, write a local HTTP
test fixture and get the *current* in-house implementation under
test.  If we can't measure the old behaviour, we can't tell whether
the new behaviour matches it — and the temptation to declare victory
on "looks fine on my box" is high.  A baseline of "old code passes
N/M cases" is the deliverable of milestone 1.

Then port one call site at a time, smallest first.  External-IP
detection is the natural first migration: one GET, no UI, no
fallback chain, easy to A/B against the live services.  Server list
next (GET + parse), then announce (POST), then pakset download last
(progress callback, cancellation, the largest behavioural surface).

One commit per call site so bisect lands on the exact regression
site.  Don't delete the old `network_http_get*` symbols until all
four sites are migrated; leave them as `dbg->fatal("legacy HTTP
path; should be unreachable")` tripwires for one release cycle, in
the spirit of the rest of the port.

## Breakage matrix

The risks group as protocol-level surprises libcurl introduces that
the current code doesn't have, plus call-site-specific behaviour
that has to survive the swap.

Protocol surprises: automatic redirect following (bound MAXREDIRS,
decide policy on http→https upgrade mid-redirect); HTTP/2
negotiation (libcurl default — some servers stall); chunked transfer
encoding (libcurl handles silently, current code can't, so previously
"broken" servers will start working — surface this in commit
messages); proxy env vars (`http_proxy` / `HTTPS_PROXY` / `NO_PROXY`
picked up automatically — decide once whether to respect or disable
and document); TLS verify (default ON; first user with clock skew or
a corp MITM sees new failures); IPv6 (libcurl happily returns IPv6
where current code is IPv4-only — external-IP query in particular
needs an explicit family-preference policy).

Per call site: pakset download needs progress callback wiring and
mid-stream cancel, on a 10–100 MB body; server announce must remain
fire-and-forget and not block the sim tick on a timeout; server list
parsing must see a complete body before parse (no partial reads
leaking through); external IP needs the IPv4/IPv6 decision above to
match what downstream code expects.

Lifecycle: `curl_global_init` is not thread-safe and must run
pre-thread-spawn — placement matters because simutrans spawns a
network thread.  Re-entry: server browser refresh while announce
thread runs.  Memory: easy_handle cleanup on the cancellation path.

Platforms: Linux uses system OpenSSL + `/etc/ssl/certs` and ships no
bundle.  macOS uses SecureTransport via system curl, also no bundle.
Windows uses Schannel (vcpkg's curl default) — different cert
behaviour from OpenSSL, worth one explicit test.  Android keeps the
JNI path, no curl link there.

## Test plan

Three layers, cheap to expensive.

*Layer 1 — local HTTP fixture.*  `tools/http_fixture/server.py`,
launched on `127.0.0.1:<random-port>` by the e2e driver.  Prints
`FIXTURE_LISTENING host port` on stdout once accept() is up so the
driver can pick up the port, and logs every request to stderr with
the `FIXTURE:` prefix.  Serves only what the suite actually
asserts on today: `POST /announce` (logged as `ANNOUNCE body=...`
for payload assertions) and `GET /get_IP.php` (returns
`"127.0.0.1\n"`).  Anything else gets a 404.  When future tests
need additional routes — gzip, chunked, slow-drip, redirect chains
— add them then, not now; the fixture is meant to track real
coverage, not aspirational coverage.

*Layer 1b — e2e driver.*  `tools/http_fixture/test_e2e.py` spawns
the fixture, then launches a headless simutrans (built with
`SIMUTRANS_BACKEND=none` in `build-headless/`) with `-listserver
127.0.0.1:<port>` and `-ip_query_host 127.0.0.1:<port>` to
redirect HTTP traffic.  Two tests pass against the in-house code
today, both via real production CLI flags:
  - `test_announce` (`-server 13353 -announce`) exercises
    `network_http_post` via `karte_t::announce_server`; asserts
    the announce body carries the expected keys.
  - `test_external_ip` (`-easyserver`) exercises `network_http_get`
    via `get_external_IP`; asserts the fixture is hit and that the
    IP it returns round-trips into the follow-up announce.
Full run ~20s, dominated by pak loading.

`network_http_get_file` (used only by the GUI pakset installer)
isn't reachable headlessly without an honest CLI seam and is
deliberately uncovered until one exists.

*Layer 2 — end-to-end against pinned real URLs.*  A small set of
known-stable endpoints (a pak64 mirror, the listserver, one HTTPS
pakset host) hit in a `--network-tests` opt-in target that runs
nightly, not on every push.  Failure means either we regressed or
the upstream changed; either way worth knowing, but never gates a
merge.

*Layer 3 — manual UI smoke checklist.*  Three flows, written down
and signed off before any release that ships the new code:
pakinstaller end-to-end with progress and cancel; server announce
visible in a second instance's browser; server browser refresh
across an offline→online transition and on an IPv6-only host.  This
sandbox has no human at the keyboard; the checklist is for the
person merging upstream, not for CI.

## Quality gates

Land libcurl behind `option(USE_CURL "..." ON)` for one release
cycle.  External network behaviour is impossible to fully verify in
CI, and the rollback path has to be cheaper than a revert.  This
contradicts the no-feature-flag norm elsewhere in the port; the
justification is the impossibility of full pre-merge verification,
not a habit to repeat.

CI: the layer-1 fixture suite runs under the existing clang + ASAN
+ UBSAN job.  No new sanitizer config, just exercise the new
surface under the existing one.

Commits: one per call-site migration, in the order above.  Old
symbols stay as fatal tripwires until all four are migrated and a
release has shipped.

## Open questions

Proxy policy.  libcurl respects `http_proxy` / `HTTPS_PROXY` /
`NO_PROXY` by default.  Do we want this, or do we want
`CURLOPT_PROXY` set to empty to disable?  Default-respect matches
user expectation in corporate environments; default-disable matches
the current code (which ignores env entirely).  Decide before
milestone 2.

IPv6 preference for external-IP detection.  Today the call exists
specifically to surface a public IPv4 to the user.  With libcurl on
dual-stack we'll get whichever the OS prefers.  Either set
`CURLOPT_IPRESOLVE = CURL_IPRESOLVE_V4` for that one call, or
restructure the call site to ask for both families and present
both.  Decide alongside the external-IP migration commit.

CA bundle on Windows.  vcpkg's curl builds against Schannel by
default which uses the Windows cert store; if a packager builds
against OpenSSL instead they need a bundle.  Document the supported
configuration; don't ship a bundle ourselves.

Android.  Current plan is to keep JNI under `#ifdef __ANDROID__`.
If someone wants to consolidate later, NDK builds of libcurl exist
but the JNI path is already working — no reason to churn it now.
