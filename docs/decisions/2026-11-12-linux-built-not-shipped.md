# Linux is built, not shipped

- **Decided:** CI builds 2P Start on Linux with no game, and runs its tests, on every pull request,
  so the stand-ins another system starts from keep building. It builds on Ubuntu 26.04's own
  packages, not vcpkg. No Linux package is made or released.
- **Date:** 2026-11-12
- **Why:** a stand-in nobody builds stops building, and the person porting finds out first. Ubuntu
  26.04's OpenSSL is 3.2 or later, which Argon2id needs, and 24.04's is 3.0. Its own packages are
  quicker than vcpkg, leave the Mac and Windows builds as they are, and are what a person porting to
  Linux would use. A Linux package is for whoever ports it.
- **Numbers:** #16, "Linux is built and tested in CI, so the stand-ins keep building".
- **Replaces:** none.
