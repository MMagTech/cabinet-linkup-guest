# GStreamer from its own installer

- **Decided:** GStreamer is GStreamer's own build, from its own site, on every system: the same
  version, held to its checksum in `.github/workflows/build.yml`. The app uses GStreamer's OpenSSL, so
  the program and the packages carry one OpenSSL. Each package carries only the parts of GStreamer a
  game uses.
- **Date:** 2026-11-11
- **Why:** one version from one place behaves the same on a Mac and on Windows, and two OpenSSLs in
  one program is trouble waiting.
- **Numbers:** #8, "Piece D: the game on the Mac"; #10, "Piece E: the game on Windows, and the zip".
- **Replaces:** none.
