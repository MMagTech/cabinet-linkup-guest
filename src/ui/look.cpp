// What the menus are drawn in (look.h).

#include "ui/look.h"

namespace look {

namespace {

struct Entry {
    const char* name;
    const char* word;
    uint32_t top, mid, bottom, surface;
};

// In the console's order (look.cpp). Purple is the brand's.
constexpr Entry kColours[kColourCount] = {
    {"Purple", "purple", 0x3A2268, 0x120C26, 0x090614, 0x241A3D},
    {"Blue", "blue", 0x1C3466, 0x0A1226, 0x050914, 0x18263F},
    {"Teal", "teal", 0x144E56, 0x061B1F, 0x030D10, 0x143236},
    {"Green", "green", 0x1F4A2C, 0x0A1A0F, 0x050D07, 0x18301F},
    {"Amber", "amber", 0x5A3514, 0x1E1208, 0x0F0904, 0x3A2614},
    {"Red", "red", 0x581618, 0x1C0708, 0x0E0405, 0x381516},
    {"Wine", "wine", 0x5A1C3A, 0x1E0A14, 0x10050A, 0x3A1828},
    {"Graphite", "graphite", 0x34363C, 0x121316, 0x08090A, 0x26282D},
    {"Pink", "pink", 0xC2367E, 0x3A0F28, 0x1A0612, 0x5A1A3E},
    {"Sky", "sky", 0x1E7FD0, 0x0A2644, 0x051222, 0x1A3F66},
    {"Lime", "lime", 0x3F8A1C, 0x14300A, 0x091705, 0x2A5016},
    {"Orange", "orange", 0xC0581A, 0x3E1C08, 0x1C0D04, 0x5E3010},
    {"Sunset", "sunset", 0xD9487A, 0x3B1650, 0x0E0A26, 0x4A1E4E},
    {"Ocean", "ocean", 0x2A8FD6, 0x0B3A48, 0x06102A, 0x163E52},
    {"Aurora", "aurora", 0x2E9A5A, 0x0C3A44, 0x1A0C34, 0x183E46},
    {"Fire", "fire", 0xD0661A, 0x4A1010, 0x120606, 0x4E1A12},
};

const Entry& entry(Colour c) {
    const int i = static_cast<int>(c);
    return kColours[i >= 0 && i < kColourCount ? i : 0];
}

}  // namespace

const char* colourName(Colour c) { return entry(c).name; }
const char* colourWord(Colour c) { return entry(c).word; }

ui::Look of(Colour c) {
    const Entry& e = entry(c);
    return {ui::Color::rgb(e.top), ui::Color::rgb(e.mid), ui::Color::rgb(e.bottom), ui::Color::rgb(e.surface)};
}

}  // namespace look
