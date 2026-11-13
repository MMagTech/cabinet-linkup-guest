# Working on 2P Start

This is for people who want to build 2P Start, try it, or change one thing,
and for anyone making a 2P Start of their own for another system or frontend.
Read [The rules](#the-rules) below, then
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for where each job lives, and
[docs/decisions/](docs/decisions/README.md) for what was decided and why. AI
tools start at [AGENTS.md](AGENTS.md).

## The rules

- **It only joins.** 2P Start never hosts, and never grows into a second way
  to run CabinetOS ([guest-only](docs/decisions/2026-11-10-guest-only.md)).
  The host's console stays in charge. Anything of yours that joins a LinkUp
  server sends `"hosts": false`
  ([says-it-only-joins](docs/decisions/2026-11-12-says-it-only-joins.md)).
- **One copy of the protocol.** Nothing in `cabinetos/` is edited here: a
  change to the console's code goes to CabinetOS, and the pin moves.
- **Every screen is the console's.** Its numbers from the console's code, its
  words from the console's `linkuprules.cpp`
  ([screens-from-the-console](docs/decisions/2026-11-10-screens-from-the-console.md)).
- **Search before you write.** The console's code may already do it.
- **One job, one file.** A new job gets a file of its own, named for the job,
  never a few hundred lines added to a file that does something else.
  [ARCHITECTURE.md](docs/ARCHITECTURE.md), "Where new code goes".
- **Remove what you replace**, in the same change: the old function, its
  callers, its flag, its comment. No commented-out code.
- **Docs change with the code.** A change that adds, moves or renames a file
  updates `docs/ARCHITECTURE.md`. A comment or doc that names a file is put
  right when the file moves: a stale comment reads exactly like a current one.
- **A decision between real options** gets a small file in `docs/decisions/`.
- **Nothing private in a tracked file.** The repository is public: no
  address, host name, password, key, token or home path of anybody's machine.
- **Write plainly**, in short words.

## Pull requests and issues

- **Ask first for anything new.** A fix of a few lines that puts back what was
  meant to work goes straight to a pull request. A feature, a new system, a
  new file or folder, a change to the build or CI, or a refactor across files
  needs an issue MMagTech has agreed to first, linked from the pull request.
- **A 2P Start for another system or frontend** starts with the "A 2P Start
  for another system" issue form, whether it would live here or in a
  repository of your own. Yours is yours to name, but not "CabinetOS 2P
  Start": say it works with CabinetOS LinkUp.
- **AI-assisted pull requests are welcome**; 2P Start is built with AI too. A
  person reads it, sends it and answers for it, and the pull request says
  which tool helped and what was checked.
- **Security problems** are reported privately: [SECURITY.md](SECURITY.md).

## Where it stands

- [x] The protocol code building and tested on Windows and macOS
- [x] Pairing with a LinkUp server
- [x] The screens, as a gallery
- [x] The app's window on macOS: pairing, the list of friends, an invite, Settings
- [x] Picture, sound and a controller on macOS
- [x] The Mac app, in a DMG, with its menu bar icon
- [x] The same on Windows, with its Setup and its tray

## How it is built

It reuses the console's own LinkUp code from
[CabinetOS](https://github.com/MMagTech/cabinetos), brought in at a pinned
commit in `cabinetos/`, so there is one copy of the protocol. What a console
and a server say to each other is
[PROTOCOL.md](https://github.com/MMagTech/cabinet-linkup/blob/main/PROTOCOL.md)
in cabinet-linkup.

## Building

On a Mac, with Homebrew, and GStreamer's own runtime and development packages for macOS, version
1.28.8, from [gstreamer.freedesktop.org](https://gstreamer.freedesktop.org/download/):

```bash
brew install cmake ninja openssl@3 json-c sdl3 sdl3_ttf
git submodule update --init
cmake --preset default && cmake --build --preset default && ctest --preset default
```

`-DTWOPSTART_GAME=OFF` builds everything but the game, with no GStreamer. With the game, the app uses
GStreamer's own OpenSSL, on a Mac as on Windows; Homebrew's is for a build without it.

The Mac app as friends get it takes vcpkg, with `VCPKG_ROOT` set, in place of Homebrew's libraries:
`cmake --preset mac && cmake --build --preset mac`, then `cmake --install build-mac --prefix <folder>`
makes `2P Start.app` there, for Apple silicon and macOS 13 or later: the app, its font, and only the
parts of GStreamer a game uses, so it runs on a Mac with nothing installed.
`ci/mac-package.sh` puts it in a DMG. Each CI run keeps the DMG as `2pstart-mac`.

On Windows it takes Visual Studio 2022, vcpkg and GStreamer 1.28.8's own installer, whose OpenSSL
the app uses. [The workflow](.github/workflows/build.yml) is the worked example. After the build,
`cmake --install build --config Release --prefix <folder>` makes the app's folder: the app, its font,
only the parts of GStreamer a game uses, and Microsoft's C++ runtime, so it runs on a PC with nothing
installed. Inno Setup 6 puts that in a Setup ([windows-setup.iss](ci/windows-setup.iss)), with its
uninstaller. Each CI run on Windows installs it, tries it and uninstalls it, and keeps the Setup as
`2pstart-windows`.

The picture is decoded by the computer's graphics chip: VideoToolbox on a Mac, Direct3D 11 on
Windows. Nearly every Mac and PC has one for H.264, and most from about 2016 on for H.265, which
the host then sends, as to a console: a clearer picture for the same data. There is no decoder in
software.

## What CI checks

Every pull request and every push to `main`, on macOS and on Windows: the build with no new
warnings, the tests, the screens as pictures, and each package tried on its own with no GStreamer of
the computer's own (`ci/check-app.sh` for the Mac app, `ci/check-setup.ps1` for the Setup, which it
also uninstalls). And a read of every tracked file for anything private (`ci/check-no-private.sh`).

On Linux, which is not shipped: the build with no game (`-DTWOPSTART_GAME=OFF`), the tests and the
screens, on Ubuntu 26.04's own packages, which the workflow's Linux job names. It keeps the stand-ins
another system starts from building ([docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), "Each system's
own").

Two more hold the structure, as CabinetOS learned to
([checks-hold-the-structure](docs/decisions/2026-11-12-checks-hold-the-structure.md)):
`ci/check-long-files.sh` lists every source file over 1,500 lines, and never fails; and
`ci/check-named-files.py` reads every doc and comment for a file of ours, or of the console's at its
pin, that is not there. It warns until the date set in the workflow, then fails. Run both by hand
before a pull request:

```bash
ci/check-long-files.sh && python3 ci/check-named-files.py
```

## Releases

The version is the one in `CMakeLists.txt`. A tag `v<version>` on `main` makes a draft release with
both packages, `2P-Start-<version>-Mac.dmg` and `2P-Start-<version>-Windows-Setup.exe`, and the words
in `ci/release-notes.md`; MMagTech publishes it once he has tried them. Neither package is signed:
the README says how a friend opens each the first time. Check for updates, in the app, reads the
newest published release.

## Trying it

`2pstart` is the app. It opens a window, pairs on first run and keeps what it holds in a folder of
its own; `--folder <path>` keeps it somewhere else instead, to try it as a second friend, and leaves
the computer's Startup entry alone. `--try <file>` plays steps from a file into it and saves
pictures: [trying.h](src/app/trying.h) says how. `--check-game` says what this computer lacks to
play, a line each, with no window. On Windows, opened with no terminal, what it says as it runs goes
to `2pstart.log` in its folder. With `TWOPSTART_H264=1` set, it asks the host for H.264 only, to try
the two side by side.

The close button leaves it running, so invites still reach the friend: in the tray on Windows, in
the menu bar on a Mac, whose icon has a dot, green for Online and amber for an invite, and a menu of
its own. Settings, System: Keep running, which Off makes the close button quit; Startup, which opens
it at login with no window (`--startup`); and Check for updates, which asks GitHub for the newest
release, by itself a minute after it opens and then each day. Friends see it Offline after 15
minutes with nobody at the computer, as they would a console.

`2pstart-gallery` shows every screen with made-up friends, before any of it is wired to a server:
Left and Right step through them. `2pstart-gallery --shots <folder>` saves a picture of each. The
pictures in the README come from it.

`2pstart-cli` is the app's server side on the command line, for trying a real LinkUp server by hand
before there is a window: `pair`, `friends`, `online` and `leave`.

Every icon is drawn by [icon/make.swift](icon/make.swift): `swift icon/make.swift icon`.

## Licences

The app's own code is MIT ([LICENSE](LICENSE)). Each package carries the licence text of every part
inside it, in `licenses`, and `cmake/Licences.cmake` checks them against Settings, Credits
(`src/screens/credits.h`) each time the build is configured: a part with no line, or a line with no
text, stops the build. A part added to a package gets both.
