#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

zip=simupak128.Britain-Ex-nightly.zip
pakdir=pak128.Britain-Ex

old_sha=""
if [ -f "$zip" ]; then
	old_sha="$(sha256sum "$zip")"
fi

curl -L --fail -z "$zip" -o "$zip" \
	https://github.com/SupraSummus/hextrans-pak128.britain/releases/download/Nightly/simupak128.Britain-Ex-nightly.zip

new_sha="$(sha256sum "$zip")"
if [ "$old_sha" != "$new_sha" ] || [ ! -d "simutrans/$pakdir" ]; then
	bsdtar -xf "$zip" -C simutrans
fi

cmake --build build -j "$(nproc)"

exec build/simutrans/simutrans \
	-set_basedir simutrans \
	-set_pakdir "$pakdir/" \
	"$@"
