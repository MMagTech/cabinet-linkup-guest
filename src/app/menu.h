// The system's own menu for a line being typed into, as any app's text box opens on a right-click:
// Cut, Copy, Paste and Select All, in the system's own words, at the pointer. An NSMenu on a Mac
// (menu_mac.mm), a popup menu on Windows (menu_win.cpp), and none anywhere else (menu_none.cpp).

#pragma once

#include <SDL3/SDL.h>

namespace menu {

// What is done to the line: from the menu, or from its keys.
enum class Edit { None, Cut, Copy, Paste, SelectAll };

// Which can be chosen now: the rest are shown greyed.
struct Can {
    bool cut = false, copy = false, paste = false, selectAll = false;
};

// Open until something is chosen or a click elsewhere closes it: what was chosen, or None.
Edit edit(SDL_Window* window, const Can& can);

}  // namespace menu
