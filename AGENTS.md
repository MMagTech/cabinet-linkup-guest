# For AI coding tools

CabinetOS 2P Start lets a friend with no CabinetOS console join a LinkUp game as player two. It only
joins, and never hosts. A person reviews every pull request. Read these before changing anything:

1. `CONTRIBUTING.md`: building, testing, what CI checks, and "The rules", which apply to every tool.
2. `docs/ARCHITECTURE.md`: where each job lives, what each part must never do, and each system's own
   places.
3. `docs/decisions/README.md`: what was decided and why. Look there before proposing a change.

## Before you write code

- **A small fix needs no issue:** a few lines that put back what was meant to work.
- **Anything new needs an issue MMagTech has agreed to first:** a feature, a new system, a new file or
  folder, a change to the build or CI, a refactor across files. Ask your person to open it; you may
  draft it.
- **Nothing in `cabinetos/` is edited.** It is CabinetOS at a pinned commit, the one copy of the
  protocol.
- **It only joins.** Nothing you write hosts a game, and anything that joins a server sends
  `"hosts": false`.

## Before you say it is done

- Build and test: `cmake --preset default && cmake --build --preset default && ctest --preset default`.
  `-DTWOPSTART_GAME=OFF` builds everything but the game, with no GStreamer.
- A change to a screen: `2pstart-gallery --shots <folder>`, and look at the pictures.
- `ci/check-no-private.sh`, `ci/check-long-files.sh` and `python3 ci/check-named-files.py`.
- List every file you created, deleted or moved, and check `docs/ARCHITECTURE.md` still matches. If a
  check fails, say so; never work around it.
