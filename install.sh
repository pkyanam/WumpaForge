#!/bin/bash
# Download the public source to a fresh local folder, then select an ISO and build.
set -euo pipefail
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    echo 'Use an Apple Silicon Mac with Terminal running natively.' >&2
    exit 1
fi
parent="${WUMPAFORGE_PARENT:-$HOME/Applications}"
mkdir -p "$parent"
checkout="$(mktemp -d "$parent/WumpaForge-build.XXXXXX")"
archive="$(mktemp -t wumpaforge-source)"
trap 'rm -f "$archive"' EXIT
curl --fail --location --show-error --silent https://github.com/pkyanam/WumpaForge/archive/refs/heads/main.tar.gz -o "$archive"
tar -xzf "$archive" -C "$checkout" --strip-components=1
rm -f "$archive"
trap - EXIT
printf 'Source and build files: %s\n' "$checkout"
exec /bin/bash "$checkout/setup.sh" "$@"
