# Variations by a written contract, not plugins

- **Decided:** people who want a 2P Start for another system (Linux, Android, iOS) or a guest inside
  another frontend are helped by written docs and tests: the map in `docs/ARCHITECTURE.md`, the
  places each system fills in, and the protocol in cabinet-linkup. There is no plugin system. Every
  variation only joins and sends `"hosts": false`.
- **Date:** 2026-11-12
- **Why:** a plugin system invites features beyond joining, against [guest-only](2026-11-10-guest-only.md),
  and the places each system fills in already do that job when the app is built. A frontend that
  does not share this code needs the protocol written down, not a plugin.
- **Numbers:** none.
- **Replaces:** none.
