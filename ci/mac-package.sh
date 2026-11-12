#!/usr/bin/env bash
#
# The Mac app as friends download it: in a DMG, with Applications beside it to drag it onto. Like the
# Windows Setup, it is not signed for other computers: it keeps the signature the install gave it
# (cmake/MacLibraries.cmake), and a friend's Mac asks before it first opens it (ci/release-notes.md
# says what to press).
#
#   ci/mac-package.sh <the app> <the DMG to make>
set -euo pipefail

app="$1"
dmg="$2"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

ditto "$app" "$stage/$(basename "$app")"
ln -s /Applications "$stage/Applications"
rm -f "$dmg"
hdiutil create -volname "2P Start" -srcfolder "$stage" -fs HFS+ -format UDZO -ov "$dmg" >/dev/null
echo "made $dmg"
