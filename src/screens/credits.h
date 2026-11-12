// CREDITS AND LICENSES, as the console has them (frontend/src/credits.h): one line for each
// project whose work is in this app, what it does, and its licence. The words on screen are the
// console's, "licenses" included. Each package carries every one's licence text in its licenses
// folder (CMakeLists.txt, "Licences"), which is checked against this list when it is made.

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
    {"libpng", "Inside FreeType \xC2\xB7 libpng"},
    {"bzip2", "Inside FreeType \xC2\xB7 bzip2"},
    {"Brotli", "Inside FreeType \xC2\xB7 MIT"},
    {"Noto Sans", "Font \xC2\xB7 OFL 1.1"},
    {"GStreamer", "Picture and sound \xC2\xB7 LGPL v2+"},
    {"GLib", "Inside GStreamer \xC2\xB7 LGPL v2+"},
    {"libnice", "Connection \xC2\xB7 MPL 1.1, LGPL v2.1"},
    {"libsrtp", "Inside GStreamer \xC2\xB7 BSD 3-clause"},
    {"usrsctp", "Inside GStreamer \xC2\xB7 BSD 3-clause"},
    {"Opus", "Sound \xC2\xB7 BSD 3-clause"},
    {"ORC", "Inside GStreamer \xC2\xB7 BSD 2-clause"},
    {"PCRE2", "Inside GLib \xC2\xB7 BSD 3-clause"},
    {"libffi", "Inside GLib \xC2\xB7 MIT"},
    {"proxy-libintl", "Inside GLib \xC2\xB7 LGPL v2+"},
    {"MoltenVK", "Inside GStreamer, on a Mac \xC2\xB7 Apache 2.0"},
    {"OpenSSL", "Encryption \xC2\xB7 Apache 2.0"},
    {"curl", "Network \xC2\xB7 curl License"},
    {"json-c", "JSON \xC2\xB7 MIT"},
    {"zlib", "Compression \xC2\xB7 zlib"},
};

}  // namespace screens
