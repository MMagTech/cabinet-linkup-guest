# Decisions

What 2P Start decided and why, one small file for each decision, as in CabinetOS's
`docs/decisions/`. Before proposing a change, look here for the decision that already covers it.

## When to write one

When you or MMagTech chose between real options and a later reader would ask why it is this way.
Not for a bug fix, a choice with no alternative, research, a fault or a build log.

## The file

Named `YYYY-MM-DD-short-name.md`, at most 60 lines, in this form:

```
# The decision in a few words

- **Decided:** what, in one to three sentences.
- **Date:** YYYY-MM-DD
- **Why:** the reason a later reader needs.
- **Numbers:** the issues and pull requests, each with its title, or "none".
- **Replaces:** the decision it replaces, or "none".
```

A decision that no longer holds is rewritten or deleted in the same pull request, and its line in the
index with it.

## Index

One line per file, in the form `- [name](file.md): the decision in a few words.`

### What 2P Start is

- [guest-only](2026-11-10-guest-only.md): 2P Start joins a game and never hosts; the host's console stays in charge.
- [says-it-only-joins](2026-11-12-says-it-only-joins.md): 2P Start tells a server it only joins, with `"hosts": false`, so it is never offered to another as a friend.
- [one-server-at-a-time](2026-11-10-one-server-at-a-time.md): one LinkUp server at a time, as a console has; several at once dropped, since linked servers cover it.
- [variations-by-a-written-contract](2026-11-12-variations-by-a-written-contract.md): other systems and frontends are helped by written docs and tests, not by a plugin system.

### Screens

- [screens-from-the-console](2026-11-10-screens-from-the-console.md): every screen takes its numbers from the console's code and its words from the console's `linkuprules.cpp`.

### The game

- [graphics-chip-decodes](2026-11-11-graphics-chip-decodes.md): the picture is decoded by the computer's graphics chip only, with no decoder in software.
- [gstreamer-from-its-own-installer](2026-11-11-gstreamer-from-its-own-installer.md): GStreamer is GStreamer's own build, the same version from its own site on every system, and its OpenSSL is the app's.

### Packages and the project

- [mac-ships-unsigned](2026-11-12-mac-ships-unsigned.md): the Mac app is not signed by Apple or notarized; friends press Open Anyway once.
- [checks-hold-the-structure](2026-11-12-checks-hold-the-structure.md): CI reads for long files and for docs and comments that name what is not there, as CabinetOS learned to.
- [linux-built-not-shipped](2026-11-12-linux-built-not-shipped.md): CI builds and tests 2P Start on Linux with no game, on Ubuntu's own packages; no Linux package is made.
- [porting-guide-a-page-of-its-own](2026-11-13-porting-guide-a-page-of-its-own.md): what someone porting 2P Start needs is one page, `docs/PORTING.md`, not a section of `CONTRIBUTING.md`.
- [ask-first-only-to-add-here](2026-11-13-ask-first-only-to-add-here.md): a copy of one's own needs no one's leave; a new system to be added here is asked about first, on an issue, before the work.
- [hears-of-fixes](2026-11-13-hears-of-fixes.md): a weekly check says when GStreamer has a newer version with fixes, Dependabot watches GitHub's actions monthly, and vcpkg's libraries and the `cabinetos` pin are left as they are.
