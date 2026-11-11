# CabinetOS 2P Start

**Player two, from anywhere.**

**This is early and does not work yet.** Nothing here can be installed or
played today. It is being built in the open.

2P Start is a small app for a friend who has no CabinetOS console. It joins a
LinkUp game that a CabinetOS console hosts: the friend sees and hears the
host's game and plays as player two with their own controller. It only joins,
and never hosts. The game, the saves and the library stay on the host's
console, and the friend needs none of them.

## Why it exists

CabinetOS is a console under a TV, and that does not change. But not every
friend can give a PC over to one. 2P Start is a thank-you to the people who
run CabinetOS: a handful of quarters to hand a buddy. You bring the console
and the games. They just press Start.

## Where it stands

- [ ] The protocol code building and tested on Windows and macOS
- [ ] Pairing with a LinkUp server
- [ ] Picture, sound and a controller on macOS
- [ ] The same on Windows
- [ ] A Windows package

Windows comes first, then macOS.

## How it is built

It reuses the console's own LinkUp code from
[CabinetOS](https://github.com/MMagTech/cabinetos), brought in at a pinned
commit in `cabinetos/`, so there is one copy of the protocol. What a console
and a server say to each other is
[PROTOCOL.md](https://github.com/MMagTech/cabinet-linkup/blob/main/PROTOCOL.md)
in cabinet-linkup.

## Licence

MIT. See [LICENSE](LICENSE).
