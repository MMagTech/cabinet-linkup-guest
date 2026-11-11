# CabinetOS 2P Start

**Player two, from anywhere.**

**This is early and does not work yet.** Nothing here can be installed or
played today. It is being built in the open.

2P Start is a small app for a friend who has no CabinetOS console. It joins a
LinkUp game that a CabinetOS console hosts: the friend sees and hears the
host's game and plays as player two with their own controller. It only joins,
and never hosts. The game, the saves and the library stay on the host's
console, and the friend needs none of them.

## Why it exists

CabinetOS is a console under a TV, and that does not change. But not every
friend can give a PC over to one. 2P Start is a thank-you to the people who
run CabinetOS: a handful of quarters to hand a buddy. You bring the console
and the games. They just press Start.

## Where it stands

- [x] The protocol code building and tested on Windows and macOS
- [x] Pairing with a LinkUp server
- [x] The screens, as a gallery
- [x] The app's window on macOS: pairing, the list of friends, an invite, Settings
- [x] Picture, sound and a controller on macOS
- [ ] The same on Windows
- [ ] A Windows package

Windows comes first, then macOS.

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

`-DTWOPSTART_GAME=OFF` builds everything but the game, with no GStreamer.

On Windows it takes Visual Studio 2022, vcpkg and GStreamer 1.28, whose OpenSSL the app uses.
[The workflow](.github/workflows/build.yml) is the worked example.

`2pstart` is the app. It opens a window, pairs on first run and keeps what it holds in a folder of
its own; `--folder <path>` keeps it somewhere else instead, to try it as a second friend.
`--try <file>` plays steps from a file into it and saves pictures: [trying.h](src/app/trying.h) says how.
On Windows, until its picture and sound are built, Join tells the server and then ends the session
at once, so the host is never left waiting.

The close button leaves it running, so invites still reach the friend: in the tray on Windows, in
the Dock on a Mac. Startup, in Settings, opens it at login with no window (`--startup`). Friends see
it Offline after 15 minutes with nobody at the computer, as they would a console.

`2pstart-gallery` shows every screen with made-up friends, before any of it is wired to a server:
Left and Right step through them. `2pstart-gallery --shots <folder>` saves a picture of each.

`2pstart-cli` is the app's server side on the command line, for trying a real LinkUp server by hand
before there is a window: `pair`, `friends`, `online` and `leave`.

## Licence

MIT. See [LICENSE](LICENSE). The font in `assets/fonts` is Noto Sans, under the SIL Open Font
Licence beside it.
