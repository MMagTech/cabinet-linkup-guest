// What a friend changed in Settings (settings.h). A small JSON file, settings.json.

#include "guest/settings.h"

#include <cstdint>
#include <vector>

#include "fileio.h"
#include "jsonutil.h"

namespace guest {

namespace {

std::string fileIn(const std::string& folder) { return folder + "/settings.json"; }

}  // namespace

Settings readSettings(const std::string& folder) {
    Settings s;
    const std::vector<uint8_t> held = cab::readBytes(fileIn(folder));
    json_object* o = jsonutil::parse(std::string(held.begin(), held.end()));
    if (!o || !json_object_is_type(o, json_type_object)) {
        if (o) json_object_put(o);
        return s;
    }
    const std::string colour = jsonutil::str(o, "colour");
    if (!colour.empty()) s.colour = colour;
    s.fullScreen = jsonutil::flag(o, "full_screen");
    s.controller = jsonutil::str(o, "controller");
    s.appearOffline = jsonutil::flag(o, "appear_offline");
    s.startup = jsonutil::flag(o, "startup");
    s.saidTray = jsonutil::flag(o, "said_tray");
    json_object_put(o);
    return s;
}

bool keepSettings(const std::string& folder, const Settings& s) {
    json_object* o = json_object_new_object();
    jsonutil::set(o, "colour", s.colour);
    jsonutil::set(o, "full_screen", s.fullScreen);
    jsonutil::set(o, "controller", s.controller);
    jsonutil::set(o, "appear_offline", s.appearOffline);
    jsonutil::set(o, "startup", s.startup);
    jsonutil::set(o, "said_tray", s.saidTray);
    const std::string text = json_object_to_json_string_ext(o, JSON_C_TO_STRING_PRETTY) + std::string("\n");
    json_object_put(o);
    return cab::writeBytes(fileIn(folder), text, 0600);
}

}  // namespace guest
