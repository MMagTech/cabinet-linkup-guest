# cabinet-linkup-guest, for coding sessions

CabinetOS 2P Start: a small app that lets a friend with no CabinetOS console join a LinkUp game as
player two. It only joins, and never hosts. It is early; the [README](README.md) says where it stands.

The console's own LinkUp code comes in from CabinetOS as the `cabinetos/` submodule, at a pinned
commit, so there is one copy of the protocol. Nothing in `cabinetos/` is edited here. What a console
and a server say to each other is `PROTOCOL.md` in MMagTech/cabinet-linkup.

## The map

| Path | Its job |
|---|---|
| `cabinetos/` | CabinetOS at a pinned commit. Its `linkup/` is built as it is written |
| `CMakeLists.txt`, `cmake/` | The one build, for CI and by hand. `NoiseTestWithoutSound.cmake` leaves the sound queue out of CabinetOS's Noise test until the sound is built here |
| `src/guest/` | 2P Start's own: who the friend is to a server, and where the app keeps its folder |
| `src/cli/` | `2pstart-cli`, the app's server side on the command line |
| `src/win/` | What Windows lacks for the console's code to build as written: `fileio.h` the Windows way, and a few POSIX names |
| `tests/` | `guest-test`, with no network |
| `ci/` | What the workflow runs |

Build and test: `cmake --preset default && cmake --build --preset default && ctest --preset default`.

## Rules for every change

- Show the plan first, and wait for a yes before creating or editing anything.
- Commit or push only when asked. Never turn on Auto-fix or auto-merge on a pull request.
- This repository is public. No address, host name, password, key, token or home path of anybody's
  machines goes in a tracked file. Signing and Store credentials live in GitHub Actions secrets.
- It stays small and guest-only. The host's console stays in charge.
- Write plainly, in short words, as the CabinetOS and cabinet-linkup docs are written.
