# cabinet-linkup-guest, for coding sessions

CabinetOS 2P Start: a small app that lets a friend with no CabinetOS console join a LinkUp game as
player two. It only joins, and never hosts. The [README](README.md) is for a friend landing on it; [CONTRIBUTING.md](CONTRIBUTING.md) says where it stands and how it is built.

The console's own LinkUp code comes in from CabinetOS as the `cabinetos/` submodule, at a pinned
commit, so there is one copy of the protocol. Nothing in `cabinetos/` is edited here. What a console
and a server say to each other is `PROTOCOL.md` in MMagTech/cabinet-linkup.

Not a session with MMagTech, such as someone porting 2P Start? [AGENTS.md](AGENTS.md) and
[docs/PORTING.md](docs/PORTING.md) are yours. "Rules for every change", below, are his.

## Read first

1. [CONTRIBUTING.md](CONTRIBUTING.md): building, testing, what CI checks, and the project's rules.
2. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): the map, what each part must never do, and each
   system's own places. It is loaded below.
3. [docs/decisions/](docs/decisions/README.md): what was decided and why. Look there before proposing
   a change.
4. [docs/NEXT-SESSION.md](docs/NEXT-SESSION.md): what is being worked on now, and what waits.
   Rewrite it at the end of each piece of work, in that work's own branch.

Build and test: `cmake --preset default && cmake --build --preset default && ctest --preset default`.

## Rules for every change

MMagTech's, for sessions with him. The project's own rules, for everybody, are in CONTRIBUTING.md,
"The rules".

- Show the plan first, and wait for a yes before creating or editing anything.
- Commit or push only when asked. Never turn on Auto-fix or auto-merge on a pull request.
- This repository is public. No address, host name, password, key, token or home path of anybody's
  machines goes in a tracked file.
- MMagTech judges every screen in the gallery before anything is built on it.
- A decision between real options gets a file in docs/decisions/, in the same pull request.
- Write plainly, in short words, as the CabinetOS and cabinet-linkup docs are written.

@docs/ARCHITECTURE.md
