# 2P Start's architecture

Where each job lives, what each part must never do, and the places each system fills in. Start with
[CONTRIBUTING.md](../CONTRIBUTING.md) if you have not.

**When you change the layout, change this file in the same pull request**: a new file, a moved job, a
renamed part, or a change to who calls whom.

## The map

| Path | Its job |
|---|---|
| `cabinetos/` | CabinetOS at a pinned commit. Its `linkup/` is built as it is written, and so are the console's LinkUp words (`linkuprules.cpp`) and interface sounds (`sound.cpp`) |
| `CMakeLists.txt`, `cmake/` | The one build, for CI and by hand. `SoundQueue.cmake` cuts the console's sound queue out of `friend_sound.cpp`, where it sits beside PipeWire, for the game and for CabinetOS's Noise test, which runs whole. `WindowsLibraries.cmake` finds the libraries the Windows app carries, and `MacLibraries.cmake` those the Mac app carries, cut to Apple silicon. `Licences.cmake` puts each part's licence in both, and checks them against the Credits. The rest are what each package is made from: the Windows icons and version, the Mac app's `Info.plist`, its Startup agent, and the `mac` preset's vcpkg triplet |
| `icon/` | Every icon, drawn by `make.swift` from the mark's shapes: the app's, the tray's and the menu bar's, with the status dot. `swift icon/make.swift icon` makes them again |
| `src/guest/` | 2P Start's own: who the friend is to a server, where the app keeps its folder and settings, and the friend's side of the server (`link`), a game's half of it included, with no window |
| `src/game/` | A game: the friend's side of the connection on a thread of its own (`game`), the stream with its picture (`stream`) and its sound at the host's pace (`sound`), with no window. GStreamer from its own installer; the picture decoded by the graphics chip, VideoToolbox on a Mac and Direct3D 11 on Windows. `none.cpp` stands in for a build without it (`-DTWOPSTART_GAME=OFF`) |
| `src/pipewire/` | The one piece of PipeWire the console's sound queue uses, its ring buffer, as `src/win/` stands in for Windows |
| `src/ui/` | What every screen is drawn with: rounded shapes and words on the console's 1920 by 1080 canvas (`draw`), the console's sixteen colours (`look`), the parts screens share (`parts`), and the bars and glow around a host's game (`glow`). The numbers are the console's |
| `src/screens/` | Each screen, given what to show. None knows a server, a stream or a controller |
| `src/gallery/` | `2pstart-gallery`: every screen with made-up friends, in a window or as pictures |
| `src/app/` | `2pstart`, the app: its window, what each press does, a game in it (`session.cpp`), Check for updates (`updates`), and the three kinds of hands turned into the console's moves |
| `src/app/` `idle`, `startup`, `tray`, `menu` | Where the app meets the computer, each system's own way: Offline with nobody at it, opening at login, the tray, the menu bar, the Dock and notifications, and a text box's right-click menu |
| `assets/fonts/` | Noto Sans, the console's font, with its licence |
| `src/cli/` | `2pstart-cli`, the app's server side on the command line |
| `src/win/` | What Windows lacks for the console's code to build as written: `fileio.h` the Windows way, a few POSIX names, and in `posix/` the POSIX headers `rtc.cpp` names |
| `tests/` | `guest-test` and `app-test`, with no network and no window |
| `ci/` | What the workflow runs: each package tried on its own, the DMG, the Windows Setup with its uninstaller, the release's words, and the checks every pull request runs (`check-no-private.sh`, `check-long-files.sh`, `check-named-files.py`) |
| `docs/` | This map, the [decisions](decisions/README.md), and the README's pictures (`media/`) |
| `.github/` | The workflow (`build.yml`), the issue forms and the pull request template |

## What each part must never do

The parts are built as libraries in `CMakeLists.txt`, each linking only the ones it stands on, so a
call from a part into one it must not know does not link.

| Part | Never |
|---|---|
| `cabinetos/` | Edited here. A change to the console's code goes to CabinetOS, and the pin moves |
| `src/guest/` | Knows a window, a screen or a word on screen. It says what happened, as events; the app decides what each shows |
| `src/game/` | Knows a screen. It hands the app a picture and events, and takes the pad |
| `src/ui/`, `src/screens/` | Knows a server, a stream or a controller. A screen draws what it is given and says what was pressed |
| `src/app/` | Holds a word or a number of its own where the console has one: those come from `linkuprules.cpp` and the console's code |
| Anything | Hosts a game ([guest-only](decisions/2026-11-10-guest-only.md)) |

## Where new code goes

- **A new job gets a file of its own**, named for the job, never a few hundred lines added to a file
  that does something else. The App's methods may live in a file named for their job, as a game's do
  in `session.cpp`: `app.cpp` need not hold all of them.
- **Search first.** The console's code, built from `cabinetos/`, may already do it.
- **A new folder, program or library** is asked about first, on an issue.

## Each system's own

What differs between systems sits in a few places, each with a stand-in that builds anywhere, so a
new system can run before it has all of them.

| Place | Mac and Windows | The stand-in elsewhere |
|---|---|---|
| `src/guest/paths.cpp` | Application Support; local AppData | XDG's data folder |
| `src/app/idle.cpp` | The computer says how long since a key or the mouse, and when the screen goes dark | Only the app's own presses count |
| `src/app/startup.cpp` | A Login Item; the Run key | Startup says it cannot |
| `src/app/tray_mac.mm`, `tray_win.cpp` (`tray_system.h`) | The menu bar and the Dock; the tray, with notifications | `tray_none.cpp`: the close button quits, no notifications |
| `src/app/menu_mac.mm`, `menu_win.cpp` | The system's own right-click menu for a text box | `menu_none.cpp`: none |
| `src/game/stream.cpp` | The graphics chip's decoder: `vtdec`, `d3d11h264dec` | None: `lacking()` says so, and no game is offered. A new system names its own |
| `src/game/game.cpp` | Where the package keeps GStreamer's plugins | None: GStreamer is the one installed |
| `CMakeLists.txt`, `cmake/`, `ci/` | How each package is made and tried | None: a new system adds its own, and a job in `.github/workflows/build.yml` |
