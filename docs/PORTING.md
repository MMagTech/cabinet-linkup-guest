# Porting 2P Start

For anyone making a 2P Start for another system (Linux, Android, iOS, a handheld) or a guest inside
another frontend: where to start, what each system fills in, and how a package and its CI job are
added. Read it with [ARCHITECTURE.md](ARCHITECTURE.md), the map, and
[CONTRIBUTING.md](../CONTRIBUTING.md), "The rules".

## The rules first

- **It only joins.** It never hosts a game, and holds no games, saves or library
  ([guest-only](decisions/2026-11-10-guest-only.md)). When it pairs and signs in, it sends
  `"hosts": false` ([says-it-only-joins](decisions/2026-11-12-says-it-only-joins.md); `PROTOCOL.md`
  in cabinet-linkup, "Who is joining"). Built on this code, `src/guest/link.cpp` sends it already:
  keep it.
- **It is not called "CabinetOS 2P Start".** Its name is yours: say it works with CabinetOS LinkUp. In
  a repository of your own, give it its own name and id where the code sets them, so it never shares
  a folder or a Login Item with 2P Start on the same computer: search for `CabinetOS 2P Start` and
  `io.github.mmagtech.2pstart`.
- **It starts with an issue**: the "A 2P Start for another system" form, whether it would live here
  or in a repository of your own. A new system here waits for MMagTech's yes on it, as anything new
  does.
- **Nothing in `cabinetos/` is edited.** It is the console's code at a pinned commit, the one copy of
  the protocol. A change to it goes to CabinetOS, and the pin moves.
- **Every screen is the console's**, its numbers from the console's code and its words from
  `linkuprules.cpp` ([screens-from-the-console](decisions/2026-11-10-screens-from-the-console.md)).
  A port draws what the console draws.

## Three ways to build one

The issue form asks which:

- **A new system here**, beside the Mac and Windows: the rest of this page.
- **On this code, in a repository of your own**: the same, with a name and id of its own.
- **Written anew, from the protocol**, for a frontend that does not share this code:
  [A frontend of its own](#a-frontend-of-its-own), below.

## Start from the Linux build

CI builds 2P Start on Linux on every pull request, with no game, on Ubuntu 26.04's own packages, and
runs its tests ([linux-built-not-shipped](decisions/2026-11-12-linux-built-not-shipped.md)). There,
each system's own place has its stand-in, so a new system starts from that build and fills the
places in one at a time.

The packages are the ones the workflow's Linux job installs. A computer of your own also needs a C++20
compiler and CMake 3.25 or later. OpenSSL must be 3.2 or later, for Argon2id.

```bash
sudo apt-get install -y --no-install-recommends ninja-build libssl-dev libcurl4-openssl-dev \
  libjson-c-dev libsdl3-dev libsdl3-ttf-dev
git submodule update --init
cmake --preset default -DTWOPSTART_GAME=OFF
cmake --build --preset default
ctest --preset default
```

What that gives:

- **`2pstart`**, the app. It pairs, lists friends, takes invites and keeps Settings. With no game,
  Join ends the session at once (`src/game/none.cpp`). `--folder <folder>` runs it as a second friend,
  and `--try <file>` plays steps into it and saves pictures (`src/app/trying.h`).
- **`2pstart-cli`**, the app's server side on the command line: pairing and signing in with no window.
- **`2pstart-gallery --shots <folder>`**, every screen as a picture, to see the drawing and the font
  are right on the new system.
- **The tests**: the console's Noise test, `guest-test`, `app-test` and the gallery's pictures, with
  no network and no window.

The window, the controllers, the game's sound and the words on screen come through SDL3 and
SDL3_ttf, so a system SDL3 runs on has nothing to fill in for those.

## Each system's own places

[ARCHITECTURE.md](ARCHITECTURE.md), "Each system's own", lists them, with what the Mac and Windows
do and what stands in elsewhere. A new system fills each in its own way: a branch of its own in the
same file, as `paths.cpp` has, or a file of its own chosen in `CMakeLists.txt`, as the tray's are.
What each must do:

- **Where the app keeps its folder**: `src/guest/paths.cpp`, as `paths.h` says. One folder for each
  person, in the system's usual place for an app's data. It holds the friend's key, pairing and
  settings, and nothing else in the program writes to disk. The stand-in is XDG's data folder, which
  is right for Linux.
- **Whether anybody is at the computer**: `src/app/idle.cpp`, as `idle.h` says. The seconds since the
  last key or mouse anywhere on the computer, whether the screen is locked or dark, and a wake before
  the computer sleeps, so Offline goes before the network does. The rule itself is shared and tested;
  the system fills in `Watch::System`. The stand-in counts only the app's own presses, so a person busy
  in another program shows Offline after 15 minutes.
- **Opening at login**: `src/app/startup.cpp`, as `startup.h` says. The system's own entry, set on or
  off, that starts this program where it is now with `--startup`, which opens with no window. The
  stand-in says Startup cannot. A system with no such thing keeps it.
- **The tray**: a file of its own beside `src/app/tray_mac.mm` and `src/app/tray_win.cpp`, defining
  what `tray_system.h` lists; `tray.h` says what the tray does. An icon that shows the status, its menu
  (each invite, Status, Open, Startup, Quit), the close button hiding the window while Keep running
  is on, and a notification for an invite, with Join and Decline. And `another`: one copy runs for
  each folder, and opening the app again brings back the one running. `tray_none.cpp` stands in: the
  close button quits, there are no notifications, and nothing stops a second copy on the same folder.
  Fill this one in early.
- **The right-click menu of a line being typed into**: a file beside `src/app/menu_mac.mm` and
  `src/app/menu_win.cpp`, as `menu.h` says. The system's own Cut, Copy, Paste and Select All at the
  pointer, and which was chosen. `menu_none.cpp` stands in with none; the keys still do all of it. A
  system with no right-click keeps it.
- **The game's decoder**: `src/game/stream.cpp` names the system's graphics chip's decoder in
  GStreamer, for H.264, and for H.265 where the chip has it. There is no decoder in software
  ([graphics-chip-decodes](decisions/2026-11-11-graphics-chip-decodes.md)). Until it is named,
  `lacking()` lists it and no game is offered.
- **Where a package keeps GStreamer's plugins**: `src/game/game.cpp`, `useGStreamerBeside`. A
  package that carries GStreamer names its folder there; with none, the GStreamer installed on the
  computer is used.

## The game

The game is built on the Mac and Windows only. On another system, `-DTWOPSTART_GAME=ON` needs the
build to find GStreamer there: `CMakeLists.txt` looks in the folder GStreamer's own installer makes
(`-DGSTREAMER_ROOT`), and GStreamer's own build is what 2P Start uses
([gstreamer-from-its-own-installer](decisions/2026-11-11-gstreamer-from-its-own-installer.md)).
GStreamer makes its own for Android and iOS too. Where it makes none, as for Linux, which GStreamer
to use is a choice for the issue, and a decision of its own.

`Stream::lacking()` in `src/game/stream.cpp` lists every part of GStreamer a game uses, the decoder
among them. `2pstart --check-game` prints what this copy lacks, a line each, and nothing when it can
play. What a game sends and takes is the console's own code, `cabinetos/linkup/rtc.cpp`, built as it
is written.

A game is tried for real against a console: whoever builds a 2P Start has one to host.

## A package and a CI job

The Mac and Windows packages are made the same way:

- `cmake --install` makes the package when the build has the game and vcpkg built the rest
  (`TWOPSTART_PACKAGE` in `CMakeLists.txt`). It carries the app, its font, only the plugins of
  GStreamer a game uses (the list in `CMakeLists.txt`, with the system's decoder: `applemedia` on a
  Mac, `d3d11` on Windows), the libraries those need (`cmake/MacLibraries.cmake`,
  `cmake/WindowsLibraries.cmake`), and every part's licence, checked against Credits
  (`cmake/Licences.cmake`).
- A script tries the package on a computer with nothing else of GStreamer's: it must start, and
  `--check-game` must print nothing (`ci/check-app.sh`, `ci/check-setup.ps1`).
- It is wrapped for friends: a DMG (`ci/mac-package.sh`), a Setup with its uninstaller
  (`ci/windows-setup.iss`).

A new system adds the same, in one pull request:

1. A preset in `CMakePresets.json`, as `mac` and `windows` are.
2. Its plugin list, and where its package keeps them (`CMakeLists.txt`, `src/game/game.cpp`).
3. A file in `cmake/` that finds the libraries those plugins need, as `MacLibraries.cmake` does.
4. Its section in `CMakeLists.txt` that installs the package, with the licences.
5. A script in `ci/` that tries the package on its own with `--check-game`.
6. A job in `.github/workflows/build.yml`: the build, the tests, the screens as pictures, the
   package, tried, and kept. As in the other jobs, each action is pinned to a commit, each download
   is held to its checksum, and nothing uses a secret.
7. `docs/ARCHITECTURE.md`, in the same pull request: the map, and the system's column in "Each
   system's own".

The Draft release job carries the Mac and Windows packages. A new system here is released only if
MMagTech agrees to it on its issue. In a repository of your own, the releases are yours.

## A frontend of its own

A guest inside another frontend, written anew, does not share this code. What it needs is written
down:

- **What a console and a server say**: [PROTOCOL.md](https://github.com/MMagTech/cabinet-linkup/blob/main/PROTOCOL.md)
  in cabinet-linkup. Pairing, with "Who is joining"; sessions; sealed boxes; and "Between two
  consoles: the game", for the connection, the controller, the picture and the sound.
- **The console's own code**, as a reference: `cabinetos/linkup/`, built here as it is written, with
  the Noise test. `src/guest/link.cpp` is a friend's whole side of a server in one file.
- **The same rules**: it only joins, sends `"hosts": false`, and has a name of its own.

## Before a pull request

- The build and `ctest`, on the new system.
- `2pstart-gallery --shots <folder>`, and a look at the pictures.
- `2pstart --check-game`, with the game.
- `ci/check-no-private.sh`, `ci/check-long-files.sh` and `python3 ci/check-named-files.py`.
- The pull request template, filled in: what was checked, on what, and what was not.
