# Next session

What 2P Start is working on now and what waits, for the next session to pick up. It is rewritten at
the end of each piece of work, in that work's own branch, as CabinetOS keeps its own.

Every session: [CONTRIBUTING.md](../CONTRIBUTING.md), "The rules", then
[ARCHITECTURE.md](ARCHITECTURE.md), and [decisions/](decisions/README.md) for what is decided and why.
Name every issue and pull request by its title as well as its number. Every session starts with only
`main`: a branch is deleted once it merges, on GitHub and locally.

## Where things stand

**2026-11-13.** The newest release is 2P Start 0.1.1, published on 2026-11-13, for a Mac with Apple
silicon and a Windows PC. No source file is over 1,500 lines. No pull request or issue is open.
Nothing is agreed next: ask MMagTech.

The latest merges, newest first:

- #21, "A porting guide, for a 2P Start on another system or frontend", for #20 of the same name.
  `docs/PORTING.md` is for anyone making a 2P Start for another system or frontend: the rules, the
  Linux build to start from, what each system's own place must do, the game, and how a package and
  its CI job are added. The README, `CONTRIBUTING.md`, `AGENTS.md`, the map and the issue form link to
  it ([porting-guide-a-page-of-its-own](decisions/2026-11-13-porting-guide-a-page-of-its-own.md)).
  When a place or a package changes, the guide changes with it.
- #19, "The window's code is split into files named for their jobs", for #18, "Split app.cpp into
  files named for their jobs". `src/app/app.cpp` keeps its start, the server, a frame, the hands and
  what happens; the rest of the window is in `moving.cpp`, `mouse.cpp`, `typing.cpp`,
  `updatecheck.cpp`, `controllers.cpp` and `fortray.cpp` beside it, with `shared.h`. It only moved
  code. A change that should change nothing on screen can be checked as it was: the gallery's
  pictures, and `2pstart --folder <a new folder> --try <steps>` (trying.h), saved before and after
  and compared byte for byte.
- #17, "NEXT-SESSION.md says where 2P Start stands and what waits".
- #16, "Linux is built and tested in CI, so the stand-ins keep building". Linux is built, never
  shipped.
- #15, "The rules, the map and two checks, so 2P Start stays easy to work on".

In cabinet-linkup, MMagTech/cabinet-linkup#15, "PROTOCOL.md: what two consoles say to each other once
a game starts", merged on 2026-11-13: the stream between host and friend is written down there now.

## Next

Nothing is agreed. Ask MMagTech what comes next, or wait for a friend's problem or an issue.

## Dates

- **After 2026-11-30**, `ci/check-named-files.py` fails a pull request instead of warning
  (`REPORT_ONLY_UNTIL` in `.github/workflows/build.yml`).

## Later

Kept so it is not lost, and worked on only when MMagTech raises it.

- **A link in the invite message** that opens 2P Start with the server and the code filled in.
- **A newer copy takes over from an older one still running**, so a Mac friend need not quit 2P Start
  from its menu bar before updating, as the README asks today. Only if friends hit it.
- **A smaller picture for a handheld**, only if someone builds a handheld 2P Start. The friend's
  device would ask for a picture its size; the console would have to change to send one.
