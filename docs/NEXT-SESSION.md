# Next session

What 2P Start is working on now and what waits, for the next session to pick up. It is rewritten at
the end of each piece of work, in that work's own branch, as CabinetOS keeps its own.

Every session: [CONTRIBUTING.md](../CONTRIBUTING.md), "The rules", then
[ARCHITECTURE.md](ARCHITECTURE.md), and [decisions/](decisions/README.md) for what is decided and why.
Name every issue and pull request by its title as well as its number. Every session starts with only
`main`: a branch is deleted once it merges, on GitHub and locally.

## Where things stand

**2026-11-13.** The newest release is 2P Start 0.1.1, published on 2026-11-13, for a Mac with Apple
silicon and a Windows PC. Nothing is being worked on, and no pull request or issue is open.

The latest merges, newest first:

- #16, "Linux is built and tested in CI, so the stand-ins keep building". Linux is built, never
  shipped.
- #15, "The rules, the map and two checks, so 2P Start stays easy to work on".
- #14, "2P Start 0.1.1: a friend's code works on any computer".
- #13, "2P Start says it only joins, so it is never offered to another as a friend".

## Dates

- **After 2026-11-30**, `ci/check-named-files.py` fails a pull request instead of warning
  (`REPORT_ONLY_UNTIL` in `.github/workflows/build.yml`).

## Later

Kept so it is not lost, and worked on only when MMagTech raises it.

- **Several LinkUp servers at once**
  ([one-server-at-a-time](decisions/2026-11-10-one-server-at-a-time.md)).
- **A link in the invite message** that opens 2P Start with the server and the code filled in.
- **A newer copy takes over from an older one still running**, so a Mac friend need not quit 2P Start
  from its menu bar before updating, as the README asks today. Only if friends hit it.
- **`src/app/app.cpp` split** when it next grows (1,610 lines): typing, the mouse, Check for updates,
  the controllers and the tray are each a job of their own.

Only if someone starts a 2P Start for another system or frontend
([variations-by-a-written-contract](decisions/2026-11-12-variations-by-a-written-contract.md)):

- **The stream between host and friend, written down** in cabinet-linkup's `PROTOCOL.md`: the data
  channels, the pad's bytes in a fixed order, the hello. Today it lives only in the console's code,
  `cabinetos/linkup/rtc.h`, `cabinetos/linkup/conductor.cpp` and
  `cabinetos/frontend/src/linkup_wire.h`.
- **A host with no console**, in CabinetOS, that sends a test picture and a tone, so a port can be
  proven end to end.
- **A smaller picture for a handheld.** The hello names codecs, not a size: a field added to the
  stream, on the console's side first.
- **A porting guide** beyond [ARCHITECTURE.md](ARCHITECTURE.md), "Each system's own".
