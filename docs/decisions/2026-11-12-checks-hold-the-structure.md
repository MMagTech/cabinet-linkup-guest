# Checks hold the structure, as CabinetOS learned

- **Decided:** CI reads every pull request for source files over 1,500 lines
  (`ci/check-long-files.sh`, report only) and for docs and comments that name a file that is not there
  (`ci/check-named-files.py`). The second warns until the date in `.github/workflows/build.yml`, then
  fails.
- **Date:** 2026-11-12
- **Why:** CabinetOS's `main.cpp` reached 20,018 lines one small addition at a time, because new code
  landed next to what it touched, and an audit found 33 comments and docs that had drifted. The rules
  that held were the ones CI checks; the ones only a session remembers slipped. These are the two
  checks from that work that fit an app this size.
- **Numbers:** MMagTech/cabinetos#398, "Audit the code and restructure it for contributors (main.cpp
  is 20,000 lines)"; MMagTech/cabinetos#652, "CI checks that comments name what is there, and a skill
  for the full read".
- **Replaces:** none.
