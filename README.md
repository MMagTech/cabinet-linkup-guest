# CabinetOS 2P Start

**Player two, from anywhere.**

2P Start is a small app for a friend who has no CabinetOS console. It joins a
LinkUp game that a CabinetOS console hosts: the friend sees and hears the
host's game and plays as player two with their own controller. It only joins,
and never hosts. The game, the saves and the library stay on the host's
console, and the friend needs none of them.

It is new. Get it from [Releases](https://github.com/MMagTech/cabinet-linkup-guest/releases).

## Why it exists

CabinetOS is a console under a TV, and that does not change. But not every
friend can give a PC over to one. 2P Start is a thank-you to the people who
run CabinetOS: a handful of quarters to hand a buddy. You bring the console
and the games. They just press Start.

## Installing

2P Start is free, and not signed by Apple or Microsoft, so the first time it
opens, the computer warns about it. That is expected. Each way below gets past
it once: by clicking, or with one line to copy and paste.

### On a Mac

A Mac with Apple silicon (M1 or later), on macOS 13 or later.

1. Download `2P-Start-…-Mac.dmg` from Releases and open it.
2. Drag 2P Start onto Applications.
3. Open 2P Start from Applications. The Mac says it can't check it: press Done.
4. Open System Settings, then Privacy & Security. Near the bottom, beside
   "2P Start was blocked", press Open Anyway, and give your password.

Or, in place of steps 3 and 4: open Terminal (in Applications, Utilities),
paste this line, and press Return. It takes away the mark that says 2P Start
came from the internet, for 2P Start alone.

```bash
xattr -dr com.apple.quarantine "/Applications/2P Start.app"
```

When it asks to send notifications, press Allow: that is how an invite reaches
you with the window closed. After an update, do steps 3 and 4 again, or the
line again.

### On Windows

Windows 10 or 11, 64-bit.

1. Download `2P-Start-…-Windows.zip` from Releases.
2. Unzip it anywhere, and open `2P Start.exe` in the `2P Start` folder.
3. Windows says "Windows protected your PC": press More info, then Run anyway.

Or, before you unzip it: right-click the zip, Properties, tick Unblock, and
press OK. Or open PowerShell, paste this line, and press Return. Either way,
Windows asks nothing when it opens.

```powershell
Get-ChildItem "$HOME\Downloads\2P-Start-*-Windows.zip" | Unblock-File
```

After an update, do it again.

### The first time it opens

It asks for your friend's LinkUp server, the code from their People page, and
a name. Copy and paste work in each box, and the mouse works as well as a
controller. Closing the window leaves it running, in the tray or the menu bar,
so invites still reach you; Settings, System, Keep running changes that.

## Where it stands

- [x] The protocol code building and tested on Windows and macOS
- [x] Pairing with a LinkUp server
- [x] The screens, as a gallery
- [x] The app's window on macOS: pairing, the list of friends, an invite, Settings
- [x] Picture, sound and a controller on macOS
- [x] The Mac app, in a DMG, with its menu bar icon
- [ ] The same on Windows, with its zip and its tray: built by CI, not yet run on a PC

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
`cmake --install build --config Release --prefix <folder>` makes the folder the Windows zip holds: the
app, its font, only the parts of GStreamer a game uses, and Microsoft's C++ runtime, so it runs on a
PC with nothing installed. Each CI run on Windows keeps that folder as `2pstart-windows`.

A tag `v<version>`, the version in `CMakeLists.txt`, makes a draft release with both, and the words
in `ci/release-notes.md`. Neither is signed.

The picture is decoded by the computer's graphics chip: VideoToolbox on a Mac, Direct3D 11 on
Windows. Nearly every Mac and PC has one for H.264, and most from about 2016 on for H.265, which
the host then sends, as to a console: a clearer picture for the same data. There is no decoder in
software.

`2pstart` is the app. It opens a window, pairs on first run and keeps what it holds in a folder of
its own; `--folder <path>` keeps it somewhere else instead, to try it as a second friend.
`--try <file>` plays steps from a file into it and saves pictures: [trying.h](src/app/trying.h) says how.
`--check-game` says what this computer lacks to play, a line each, with no window. On Windows,
opened with no terminal, what it says as it runs goes to `2pstart.log` in its folder. With
`TWOPSTART_H264=1` set, it asks the host for H.264 only, to try the two side by side.

The close button leaves it running, so invites still reach the friend: in the tray on Windows, in
the menu bar on a Mac, whose icon has a dot, green for Online and amber for an invite, and a menu of
its own. Settings, System: Keep running, which Off makes the close button quit; Startup, which opens
it at login with no window (`--startup`); and Check for updates, which asks GitHub for the newest
release, by itself a minute after it opens and then each day. Friends see it Offline after 15
minutes with nobody at the computer, as they would a console.

Every icon is drawn by [icon/make.swift](icon/make.swift): `swift icon/make.swift icon`.

`2pstart-gallery` shows every screen with made-up friends, before any of it is wired to a server:
Left and Right step through them. `2pstart-gallery --shots <folder>` saves a picture of each.

`2pstart-cli` is the app's server side on the command line, for trying a real LinkUp server by hand
before there is a window: `pair`, `friends`, `online` and `leave`.

## Licence

MIT. See [LICENSE](LICENSE). The font in `assets/fonts` is Noto Sans, under the SIL Open Font
Licence beside it. Each package carries the licence of every part inside it, in `licenses`, and
Settings, Credits names them.
