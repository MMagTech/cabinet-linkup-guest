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
| `CMakeLists.txt`, `cmake/` | The one build, for CI and by hand. `SoundQueue.cmake` cuts the console's sound queue out of `friend_sound.cpp`, where it sits beside PipeWire, for the game and for CabinetOS's Noise test, which runs whole |
| `src/guest/` | 2P Start's own: who the friend is to a server, where the app keeps its folder and settings, and the friend's side of the server (`link`), a game's half of it included, with no window |
| `src/game/` | A game: the friend's side of the connection on a thread of its own (`game`), the stream with its picture (`stream`) and its sound at the host's pace (`sound`), with no window. GStreamer from its own installer. `none.cpp` stands in where the game is not built yet: Windows |
| `src/pipewire/` | The one piece of PipeWire the console's sound queue uses, its ring buffer, as `src/win/` stands in for Windows |
| `src/ui/` | What every screen is drawn with: rounded shapes and words on the console's 1920 by 1080 canvas (`draw`), the console's sixteen colours (`look`), and the parts screens share (`parts`). The numbers are the console's |
| `src/screens/` | Each screen, given what to show. None knows a server, a stream or a controller |
| `src/gallery/` | `2pstart-gallery`: every screen with made-up friends, in a window or as pictures |
| `src/app/` | `2pstart`, the app: its window, what each press does, a game in it (`session.cpp`), and the three kinds of hands turned into the console's moves |
| `assets/fonts/` | Noto Sans, the console's font, with its licence |
| `src/cli/` | `2pstart-cli`, the app's server side on the command line |
| `src/win/` | What Windows lacks for the console's code to build as written: `fileio.h` the Windows way, and a few POSIX names |
| `src/app/` `idle`, `startup`, `tray` | Where the app meets the computer, each system's own way: Offline with nobody at it, opening at login, and the tray, the Dock and notifications |
| `tests/` | `guest-test` and `app-test`, with no network and no window |
| `ci/` | What the workflow runs |

Build and test: `cmake --preset default && cmake --build --preset default && ctest --preset default`.

## Rules for every change

- Show the plan first, and wait for a yes before creating or editing anything.
- Commit or push only when asked. Never turn on Auto-fix or auto-merge on a pull request.
- This repository is public. No address, host name, password, key, token or home path of anybody's
  machines goes in a tracked file. Signing and Store credentials live in GitHub Actions secrets.
- It stays small and guest-only. The host's console stays in charge.
- A screen looks and reads as LinkUp does on the console: its numbers from the console's code, and its
  words from the console's own `linkuprules.cpp`, which is built from the submodule. MMagTech judges
  every screen in the gallery before anything is built on it.
- Write plainly, in short words, as the CabinetOS and cabinet-linkup docs are written.
