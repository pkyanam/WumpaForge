#!/bin/bash
cd "$(dirname "$0")" || exit 1
./setup.sh "$@"
result=$?
if (( result != 0 )); then
    echo 'Build stopped. Read the message above, then double-click this file to retry.'
    read -r -p 'Press Return to close. '
fi
exit "$result"
