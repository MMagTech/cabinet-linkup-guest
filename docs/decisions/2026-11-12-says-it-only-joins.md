# 2P Start tells a server it only joins

- **Decided:** pairing and sign-in send `"hosts": false`, as `PROTOCOL.md` in cabinet-linkup has it
  ("Who is joining"). Anything else that only joins, a port or another frontend, sends it too.
- **Date:** 2026-11-12
- **Why:** two people who both only join can never play, and without it a server cannot tell 2P Start
  from a console, so its People page offered them to each other as friends. It is also the one thing
  that tells a server a joiner is not a console.
- **Numbers:** #13, "2P Start says it only joins, so it is never offered to another as a friend";
  MMagTech/cabinet-linkup#13, the server's half; MMagTech/cabinetos#704.
- **Replaces:** none.
