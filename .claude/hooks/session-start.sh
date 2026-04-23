#!/bin/bash
# Session-start hook for Claude Code on the web.
# Installs the system libraries Simutrans needs to compile and run its
# automated test scenario, and primes a CMake build directory.
#
# Run only inside the Claude Code remote container. On a developer's local
# machine this is a no-op so we don't surprise anyone with apt installs.

set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    echo "session-start.sh: not running in Claude Code remote env, skipping."
    exit 0
fi

echo "session-start.sh: installing Simutrans build dependencies..."

# apt: idempotent install. -y for non-interactive, -qq to keep logs short.
# Some prebaked PPAs (deadsnakes, ondrej) may be unsigned in the remote
# image and make `apt-get update` non-zero; that's fine, the main archive
# we need is still refreshed, so don't let a third-party PPA fail the hook.
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq || true
apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    autoconf \
    libbz2-dev \
    libpng-dev \
    libfreetype-dev \
    libfontconfig-dev \
    libsdl2-dev \
    libzstd-dev \
    libminiupnpc-dev \
    libfluidsynth-dev \
    moreutils

echo "session-start.sh: configuring CMake build dir (build/)..."
# Headless-friendly: SIMUTRANS_BACKEND=sdl2 is selected automatically when
# SDL2 + Freetype are present; pass -DSIMUTRANS_BACKEND=none to force a
# graphics-less build (used for fast compile-only checks).
cmake -S "$CLAUDE_PROJECT_DIR" -B "$CLAUDE_PROJECT_DIR/build" \
    -DCMAKE_BUILD_TYPE=Debug \
    >/dev/null

echo "session-start.sh: done."
