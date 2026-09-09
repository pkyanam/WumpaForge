#!/bin/bash
# Build a personal native Mac app from a user-supplied USA Xbox ISO.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
    echo 'WumpaForge requires an Apple Silicon Mac and a native Terminal (not Rosetta).' >&2
    exit 1
fi
if ! xcrun --find clang >/dev/null 2>&1; then
    echo 'macOS needs Apple Command Line Tools. Complete the installer, then run this command again.'
    xcode-select --install || true
    exit 1
fi
if [[ ! -x /opt/homebrew/bin/brew ]]; then
    echo 'Installing Homebrew build dependencies. macOS may request your administrator password.'
    installer="$(mktemp -t wumpaforge-homebrew)"
    trap 'rm -f "$installer"' EXIT
    curl --fail --location --show-error --silent https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh -o "$installer"
    /bin/bash "$installer" </dev/tty
    rm -f "$installer"
    trap - EXIT
fi
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
missing=()
for formula in python cmake sdl2 libepoxy pkg-config openssl@3; do
    if ! /opt/homebrew/bin/brew list --versions "$formula" >/dev/null 2>&1; then
        missing+=("$formula")
    fi
done
if (( ${#missing[@]} )); then
    /opt/homebrew/bin/brew install "${missing[@]}"
fi
if (( $# == 0 )); then
    iso="$(osascript -e 'POSIX path of (choose file with prompt "Choose your USA Xbox Wrath of Cortex ISO")')"
    set -- "$iso"
fi
exec /opt/homebrew/bin/python3 "$ROOT/tools/setup.py" "$@"
