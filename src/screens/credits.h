// CREDITS AND LICENSES, as the console has them (frontend/src/credits.h): one line for each
// project whose work is in this app, what it does, and its licence. The words on screen are the
// console's, "licenses" included. When the app is packaged, what goes in the package is checked
// against this list.

#pragma once

namespace screens {

struct Credit {
    const char* name;
    const char* what;
};

inline constexpr Credit kCredits[] = {
    {"CabinetOS 2P Start", "This app \xC2\xB7 MIT"},
    {"CabinetOS", "LinkUp \xC2\xB7 MIT"},
    {"SDL", "Window and controllers \xC2\xB7 zlib"},
    {"SDL_ttf", "Text \xC2\xB7 zlib"},
    {"FreeType", "Inside SDL_ttf \xC2\xB7 FreeType License"},
    {"HarfBuzz", "Inside SDL_ttf \xC2\xB7 MIT"},
    {"Noto Sans", "Font \xC2\xB7 OFL 1.1"},
    {"OpenSSL", "Encryption \xC2\xB7 Apache 2.0"},
    {"curl", "Network \xC2\xB7 curl License"},
    {"json-c", "JSON \xC2\xB7 MIT"},
};

}  // namespace screens
