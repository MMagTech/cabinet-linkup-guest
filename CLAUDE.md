# cabinet-linkup-guest, for coding sessions

CabinetOS 2P Start: a small app that lets a friend with no CabinetOS console join a LinkUp game as
player two. It only joins, and never hosts. It is early; the [README](README.md) says where it stands.

The console's own LinkUp code comes in from CabinetOS as the `cabinetos/` submodule, at a pinned
commit, so there is one copy of the protocol. Nothing in `cabinetos/` is edited here. What a console
and a server say to each other is `PROTOCOL.md` in MMagTech/cabinet-linkup.

## Rules for every change

- Show the plan first, and wait for a yes before creating or editing anything.
- Commit or push only when asked. Never turn on Auto-fix or auto-merge on a pull request.
- This repository is public. No address, host name, password, key, token or home path of anybody's
  machines goes in a tracked file. Signing and Store credentials live in GitHub Actions secrets.
- It stays small and guest-only. The host's console stays in charge.
- Write plainly, in short words, as the CabinetOS and cabinet-linkup docs are written.
