#!/usr/bin/env bash
#
# Headless Simutrans server, set up well enough for the network probes
# under tools/ (e.g. probe_nwc_tool.py). Default port 13353.
#
# Reuses the same pak64 cache as tools/test.py — first invocation pulls
# ~30 MB and skips later. Loads tests/empty-16x16.sve so the server
# skips the splash banner that would otherwise block headlessly.
#
# Usage:
#   tools/run-server.sh              # foreground, port 13353
#   tools/run-server.sh 13400        # foreground, port 13400
#   tools/run-server.sh 13353 -debug 4 ...  # extra args pass through
#
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

if [ $# -gt 0 ] && [[ "$1" =~ ^[0-9]+$ ]]; then
	PORT=$1
	shift
else
	PORT=13353
fi

# Build sim + pak + simuconf + sim symlink via test.py's idempotent
# setup. Skips work already done; on a clean checkout it fetches pak64.
python3 - <<'PY'
from tools.test import ensure_pak, ensure_simuconf, ensure_sim_symlink, build
build()
ensure_pak()
ensure_simuconf()
ensure_sim_symlink()
PY

# Stage the empty test map as a savegame the -load flag can find.
SAVE_DIR="${HOME}/simutrans/save"
mkdir -p "$SAVE_DIR"
cp -n tests/empty-16x16.sve "$SAVE_DIR/empty.sve" || true

echo "Starting Simutrans server on port $PORT (load=empty)" >&2
cd simutrans
exec env SDL_VIDEODRIVER=dummy ../sim \
	-use_workdir -objects pak -lang en -nosound -nomidi \
	-server "$PORT" -load empty -debug 3 \
	"$@"
