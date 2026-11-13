# The Mac app ships unsigned

- **Decided:** the Mac app is not signed with a Developer ID or notarized, as the Windows Setup is
  not signed either. A friend presses Open Anyway in System Settings, Privacy & Security, the first
  time and after each update, and the README says how.
- **Date:** 2026-11-12
- **Why:** Apple's signing (a certificate, an App Store Connect key, secrets in CI) cost far more than
  a few friends pressing Open Anyway. Nothing in the app is taken away.
- **Numbers:** #11, "Piece F: the packages friends download, the tray and the menu bar".
- **Replaces:** the plan of a signed, notarized Mac app.
