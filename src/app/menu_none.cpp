// No right-click menu where there is no system's own to open (menu.h): the keys still do all of it.

#include "app/menu.h"

namespace menu {

Edit edit(SDL_Window* window, const Can& can) {
    (void)window;
    (void)can;
    return Edit::None;
}

}  // namespace menu
