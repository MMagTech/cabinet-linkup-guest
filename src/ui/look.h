// What the menus are drawn in: one of the console's sixteen colours. They are CabinetOS's own
// (frontend/src/look.cpp): eight picked round the colour wheel, five bright ones, and three with a
// hue for each stop of the background. The console's dark appearance is not here: a computer has
// its own.

#pragma once

#include "ui/draw.h"

namespace look {

enum class Colour {
    Purple = 0, Blue, Teal, Green, Amber, Red, Wine, Graphite,
    Pink, Sky, Lime, Orange, Sunset, Ocean, Aurora, Fire
};
constexpr int kColourCount = 16;
const char* colourName(Colour c);   // "Purple", for the screen
const char* colourWord(Colour c);   // "purple", for the file

// The background and the panel colour for that choice.
ui::Look of(Colour c);

}  // namespace look
