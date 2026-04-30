#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

old_sha=""
if [ -f simupak128-nightly.zip ]; then
	old_sha="$(sha256sum simupak128-nightly.zip)"
fi

curl -L --fail -z simupak128-nightly.zip -o simupak128-nightly.zip \
	https://github.com/SupraSummus/hextrans-pak128/releases/download/Nightly/simupak128-nightly.zip

new_sha="$(sha256sum simupak128-nightly.zip)"
if [ "$old_sha" != "$new_sha" ] || [ ! -d simutrans/pak128 ]; then
	bsdtar -xf simupak128-nightly.zip
fi

cmake --build build -j "$(nproc)"

exec build/simutrans/simutrans \
	-set_basedir simutrans \
	-set_pakdir simutrans/pak128/ \
	"$@"
