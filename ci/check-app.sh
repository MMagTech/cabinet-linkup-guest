#!/usr/bin/env bash
#
# The Mac app, run with the GStreamer that built it moved out of the way, as on a Mac that has never
# had it: it must start and find every part a game needs inside itself, and every plugin it carries
# must load (2P Start --check-game says what it lacks, a line each). The same as ci/check-zip.ps1
# does for the Windows zip.
#
# Whether the Mac's own decoder plays a picture is seen on a real Mac; a runner may have none.
#
#   ci/check-app.sh <the app>
set -euo pipefail

app="$1"
program="$app/Contents/MacOS/2P Start"
framework=/Library/Frameworks/GStreamer.framework
away=/Library/Frameworks/GStreamer.framework.away

sudo mv "$framework" "$away"
trap 'sudo mv "$away" "$framework"' EXIT

set +e
said="$(env -i HOME="$HOME" PATH=/usr/bin:/bin "$program" --check-game 2>"$RUNNER_TEMP/check-app.err")"
code=$?
set -e
printf '%s\n' "$said" | sed '/^$/d; s/^/2P Start: /'
sed 's/^/  /' "$RUNNER_TEMP/check-app.err"

# 0 when it can play; 1 when something is lacking; anything else is macOS refusing to start it, most
# often for a library that is not there.
if [ "$code" -ne 0 ] && [ "$code" -ne 1 ]; then
    echo "2P Start did not start: exit code $code" >&2
    exit 1
fi
if [ -n "$(printf '%s' "$said" | sed '/^$/d')" ]; then
    echo "The app lacks what a game needs" >&2
    exit 1
fi
echo "The app starts on its own and has every part a game needs."
