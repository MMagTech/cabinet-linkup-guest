# How 2P Start hears of fixes in what it ships

- **Decided:**
  - **GStreamer**, which unpacks the video that comes over the network and carries the app's
    OpenSSL: a check once a week reads GStreamer's own download folders, and fails when a newer
    version in the series 2P Start is built with is out for both the Mac and Windows. The weekly
    run opens an issue for it, once (`ci/check-gstreamer.py`, `.github/workflows/gstreamer.yml`).
    It reads the packages, not GStreamer's tags: the tag comes days before the packages, and 2P
    Start moves only once both systems have them. A newer series is said, and is a choice, not a fix.
  - **GitHub's actions:** Dependabot, once a month, all in one pull request.
  - **vcpkg's libraries** (json-c, SDL3, SDL3_ttf, and curl on Windows): no `builtin-baseline`.
    Each build takes them from the runner's own vcpkg, and its log names each version.
  - **The `cabinetos` pin:** not watched. It moves on purpose, when the console's LinkUp changes.
- **Date:** 2026-11-13
- **Why:** cabinet-linkup shipped a Go with a known crash bug because nothing checked what it
  shipped. Go has govulncheck; a C++ app has no such list, but GStreamer's fixes, its security fixes
  among them, come out as newer versions in a series, so a newer one is the sign. Weekly, since the
  check is silent until a fix is out; monthly for the actions, since friends never get them.
  A baseline would hold vcpkg's libraries still and make the Mac and Windows match, but then fixes
  wait for someone to merge a pull request, and its pull request says only that vcpkg moved: GitHub
  has no security alerts for vcpkg. Left as they are, fixes arrive by themselves.
- **Numbers:** #22, "A weekly check for GStreamer's fixes, and Dependabot for the actions";
  MMagTech/cabinet-linkup#20, "Build with Go 1.27.4, which fixes a crash in net/http
  (GO-2026-6859)".
- **Replaces:** none.
