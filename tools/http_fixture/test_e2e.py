#!/usr/bin/env python3
# End-to-end test driver for the simutrans HTTP call sites.
#
# Spawns tools/http_fixture/server.py on a random local port, points
# simutrans at it via SIMUTRANS_HTTP_TEST_HOST=127.0.0.1:<port>,
# launches a headless simutrans configured to exercise one HTTP
# call site, then scrapes the fixture's stderr log to assert the
# request actually arrived in the shape we expected.
#
# Run:
#   tools/http_fixture/test_e2e.py            # all tests
#   tools/http_fixture/test_e2e.py announce   # name-substring filter
#
# Requires:
#   - build-headless/simutrans/simutrans (or build/simutrans/simutrans)
#     built from this tree; see documentation/libcurl-port.md for
#     why the headless build is preferred (no SDL2, faster startup,
#     no display needed).
#   - simutrans/pak/ populated with a pak64 (or compatible).
#     Run `cd simutrans && ../tools/get_pak.sh pak64` once if missing.

import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tools" / "http_fixture" / "server.py"
PAK_DIR = ROOT / "simutrans" / "pak"
BASE_DIR = ROOT / "simutrans"


def find_simutrans() -> Path:
    for candidate in [
        ROOT / "build-headless" / "simutrans" / "simutrans",
        ROOT / "build" / "simutrans" / "simutrans",
    ]:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    sys.exit("no simutrans binary found; build with cmake first")


def require_pakset():
    if not PAK_DIR.is_dir() or not any(PAK_DIR.iterdir()):
        sys.exit(f"no pakset at {PAK_DIR}; run "
                 "`cd simutrans && ../tools/get_pak.sh pak64` once")


class Fixture:
    """Context manager that runs server.py and exposes (host, port, lines).

    `.lines` accumulates the `FIXTURE:`-prefixed log lines as they
    arrive; tests assert on its contents after running simutrans.
    """

    def __init__(self):
        self.proc: subprocess.Popen | None = None
        self.host = ""
        self.port = 0
        self.lines: list[str] = []
        self._stderr_thread = None

    def __enter__(self) -> "Fixture":
        self.proc = subprocess.Popen(
            [sys.executable, str(FIXTURE), "--port", "0"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True,
        )
        announce = self.proc.stdout.readline().strip()
        m = re.match(r"FIXTURE_LISTENING (\S+) (\d+)", announce)
        if not m:
            self.__exit__(None, None, None)
            raise RuntimeError(f"fixture did not announce: {announce!r}")
        self.host, self.port = m.group(1), int(m.group(2))

        import threading
        def pump():
            assert self.proc and self.proc.stderr
            for line in self.proc.stderr:
                self.lines.append(line.rstrip())
        self._stderr_thread = threading.Thread(target=pump, daemon=True)
        self._stderr_thread.start()
        return self

    def __exit__(self, *exc):
        if self.proc and self.proc.poll() is None:
            self.proc.send_signal(signal.SIGINT)
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        if self._stderr_thread:
            self._stderr_thread.join(timeout=1)

    def wait_for(self, predicate, timeout=10.0) -> bool:
        """Poll self.lines until `predicate(lines)` is true or timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate(self.lines):
                return True
            time.sleep(0.1)
        return False


def run_simutrans(fixture: Fixture, extra_args: list[str], *,
                  timeout: float = 12.0) -> subprocess.CompletedProcess:
    """Launch simutrans with -listserver and -ip_query_host pointed at the fixture."""
    sim = find_simutrans()
    require_pakset()
    userdir = Path(tempfile.mkdtemp(prefix="sim-e2e-"))
    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    host = f"{fixture.host}:{fixture.port}"
    args = [
        str(sim),
        "-set_basedir", str(BASE_DIR),
        "-set_userdir", str(userdir),
        "-objects", "pak",
        "-nomidi", "-nosound", "-mute",
        "-listserver", host,
        "-ip_query_host", host,
        *extra_args,
    ]
    try:
        proc = subprocess.run(args, env=env, capture_output=True,
                              text=True, timeout=timeout)
        return proc
    except subprocess.TimeoutExpired as exc:
        # Expected outcome for tests that hold simutrans open until the
        # request fires.  Convert to a CompletedProcess for uniform handling.
        return subprocess.CompletedProcess(
            args=exc.cmd, returncode=-signal.SIGTERM,
            stdout=exc.stdout or "", stderr=exc.stderr or "",
        )
    finally:
        shutil.rmtree(userdir, ignore_errors=True)


# ---- Test cases --------------------------------------------------------


def test_announce():
    """karte_t::announce_server posts to /announce with the expected keys."""
    with Fixture() as fx:
        run_simutrans(fx, ["-server", "13353", "-announce", "-debug", "2"],
                      timeout=10.0)
        if not fx.wait_for(lambda lines: any("POST /announce" in l for l in lines),
                           timeout=1.0):
            return "no POST /announce seen; fixture log: " + repr(fx.lines)

        ann_lines = [l for l in fx.lines if l.startswith("FIXTURE: ANNOUNCE body=")]
        if not ann_lines:
            return f"announce arrived but body not logged: {fx.lines}"
        body = ann_lines[0]
        for key in ("&port=13353", "&st=1", "&rev=", "&pak=", "&size="):
            if key not in body:
                return f"announce body missing {key!r}: {body}"
    return None  # pass


def test_external_ip():
    """get_external_IP() hits /get_IP.php under -easyserver."""
    with Fixture() as fx:
        run_simutrans(fx, ["-easyserver", "-debug", "2"], timeout=10.0)
        if not any("GET /get_IP.php" in l for l in fx.lines):
            return f"no GET /get_IP.php seen; log: {fx.lines}"

        # Under -easyserver the announce body should carry the IP
        # the fixture handed back (URL-encoded; we serve "127.0.0.1\n"
        # so the in-house code passes &dns=127.0.0.1%0A — the
        # trailing newline is in-house's responsibility to strip and
        # the wart is worth baselining ahead of the libcurl port).
        ann_lines = [l for l in fx.lines if l.startswith("FIXTURE: ANNOUNCE body=")]
        if not ann_lines:
            return f"no announce after external-IP; log: {fx.lines}"
        if "&dns=127.0.0.1" not in ann_lines[0]:
            return f"announce did not carry fixture-supplied dns: {ann_lines[0]}"
    return None


# Server-browser fetch (network_http_get via server_frame_t) and pakset
# download (network_http_get_file via pak_download) are GUI-only — not
# reachable from a headless run today.  Test_announce / test_external_ip
# cover network_http_post and network_http_get respectively; coverage
# of network_http_get_file waits for an honest production CLI seam
# (e.g. pakset install from the cmdline) rather than a test-only flag.


TESTS = {
    "test_announce": test_announce,
    "test_external_ip": test_external_ip,
}


def main(argv):
    patterns = argv[1:]
    selected = {n: f for n, f in TESTS.items()
                if not patterns or any(p in n for p in patterns)}
    if not selected:
        sys.exit(f"no tests match {patterns}; available: {list(TESTS)}")

    fails = []
    for name, fn in selected.items():
        print(f"=== {name} ", end="", flush=True)
        try:
            err = fn()
        except Exception as exc:
            err = f"raised {exc!r}"
        if err is None:
            print("PASS")
        else:
            print("FAIL")
            print(f"   {err}")
            fails.append(name)

    if fails:
        print(f"\n{len(fails)}/{len(selected)} failed: {', '.join(fails)}")
        sys.exit(1)
    print(f"\n{len(selected)} test(s) passed")


if __name__ == "__main__":
    main(sys.argv)
