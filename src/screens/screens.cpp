// 2P Start's screens (screens.h).

#include "screens/screens.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

#include "screens/credits.h"
#include "ui/look.h"
#include "ui/parts.h"

namespace screens {

using ui::Color;
using ui::Rect;
using ui::TextStyle;

namespace {

// The friends list's numbers, and each friend's colour, as the console has them (linkupscreen.cpp).
constexpr float kCellGap = 24.0f;
constexpr float kTileH = 156.0f;
constexpr float kTileRadius = 18.0f;
constexpr int kColumns = 4;
constexpr float kTileFocusScale = 1.06f;
constexpr float kSwitcherGapBelow = 28.0f;
constexpr uint32_t kFriendTops[] = {0x3A2268, 0x144E56, 0x1F4A2C, 0x5A3514, 0x581618, 0x1C3466, 0xD9487A, 0x2A8FD6};
constexpr uint32_t kFriendMids[] = {0x120C26, 0x061B1F, 0x0A1A0F, 0x1E1208, 0x1C0708, 0x0A1226, 0x3B1650, 0x0B3A48};
constexpr int kFriendColours = 8;
constexpr uint32_t kOfflineTile = 0x2B2B2E;

// A colour's hue at one brightness to the eye (Oklch), so a red does not step forward of a green.
// Its strength stops at `chroma`, so a grey stays grey. The console's own sum.
Color evenColour(uint32_t rgb, float lightness, float chroma) {
    const Color col = Color::rgb(rgb);
    auto linear = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    const float lr = linear(col.r), lg = linear(col.g), lb = linear(col.b);
    const float cl = std::cbrt(0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb);
    const float cm = std::cbrt(0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb);
    const float co = std::cbrt(0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb);
    const float labA = 1.9779984951f * cl - 2.4285922050f * cm + 0.4505937099f * co;
    const float labB = 0.0259040371f * cl + 0.7827717662f * cm - 0.8086757660f * co;
    const float keep = std::min(1.0f, chroma / std::max(0.0001f, std::hypot(labA, labB)));
    const float a = labA * keep, b = labB * keep;
    const float l = lightness + 0.3963377774f * a + 0.2158037573f * b;
    const float m = lightness - 0.1055613458f * a - 0.0638541728f * b;
    const float o = lightness - 0.0894841775f * a - 1.2914855480f * b;
    const float xl = l * l * l, xm = m * m * m, xo = o * o * o;
    auto encoded = [](float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        return v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    };
    return Color{encoded(4.0767416621f * xl - 3.3077115913f * xm + 0.2309699292f * xo),
                 encoded(-1.2684380046f * xl + 2.6097574011f * xm - 0.3413193965f * xo),
                 encoded(-0.0041960863f * xl - 0.7034186147f * xm + 1.7076147010f * xo), 1.0f};
}

struct Tile {
    linkup::Status kind = linkup::Status::Offline;
    std::string name, status;
    bool dark = false;
    Color top, bottom;
};

void drawTile(ui::Canvas& c, const Tile& tile, float x, float y, float w, float h, float f) {
    using linkup::Status;
    // Solid, and everyone at one brightness, so nobody steps forward. Offline is flat and grey.
    Rect plate{x, y, w, h, kTileRadius, tile.dark ? Color::rgb(kOfflineTile) : tile.top};
    if (!tile.dark) {
        plate.gradient = true;
        plate.fillBottom = tile.bottom;
        plate.shadowBlur = 26.0f;
        plate.shadowOffsetY = 8.0f;
        plate.shadowColor = Color::black(0.40f);
    }
    c.draw(plate);
    if (!tile.dark) {
        // An invite's edge is amber and holds still.
        const bool invite = tile.kind == Status::InvitedYou;
        Rect edge{x, y, w, h, kTileRadius, Color::white(0)};
        edge.border = invite ? 3.0f : 1.5f;
        edge.borderColor = invite ? ui::palette::kMarqueeAmber : Color::white(0.10f);
        c.draw(edge);
    }
    if (f > 0.0f) {
        Rect rim{x, y, w, h, kTileRadius, Color::white(0)};
        rim.border = f * parts::kFocusRim;
        rim.borderColor = ui::palette::kFocusRim;
        c.draw(rim);
    }

    const TextStyle nameStyle = TextStyle::Title3, subStyle = TextStyle::Footnote;
    constexpr float kPad = 24.0f;
    const float textX = x + kPad, textRight = x + w - kPad;
    const float nameH = c.lineHeight(nameStyle), subH = c.lineHeight(subStyle);
    const float top = y + (h - nameH - subH - 4.0f) * 0.5f;
    c.text(c.truncate(tile.name, nameStyle, textRight - textX), textX, top + c.ascent(nameStyle), nameStyle,
           Color::white(tile.dark ? 0.55f : 1.0f));
    const float subBase = top + nameH + 4.0f + c.ascent(subStyle);
    float subX = textX;
    if (!tile.dark) {
        constexpr float kDot = 12.0f;
        const Color dot = tile.kind == Status::InvitedYou ? ui::palette::kMarqueeAmber
                          : tile.kind == Status::Online   ? ui::palette::kScreenCyan
                                                          : Color::white(0.85f);
        const float mid = subBase - c.capHeight(subStyle) * 0.5f;
        c.draw(Rect{subX, mid - kDot * 0.5f, kDot, kDot, kDot * 0.5f, dot});
        subX += kDot + 10.0f;
    }
    c.text(c.truncate(tile.status, subStyle, textRight - subX), subX, subBase, subStyle,
           Color::white(tile.dark ? 0.45f : 0.65f));
}

// The cabinet with a player two who is not here yet behind it, and what the screen is for. With
// something wrong, one cabinet, nothing lit, and the words say what.
void drawNobody(ui::Canvas& c, const std::string& instead) {
    constexpr float kCabinetH = 340.0f, kTop = 250.0f;
    const float midX = ui::kCanvasWidth * 0.5f;
    // Stars, for a screen that would otherwise stay empty: square, as pixels are, holding still,
    // the same every time, and clear of the picture and the words.
    uint32_t seed = 20261105u;
    auto next = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    for (int i = 0; i < 110; ++i) {
        const float x = static_cast<float>(next() % static_cast<uint32_t>(ui::kCanvasWidth));
        const float y = 120.0f + static_cast<float>(next() % static_cast<uint32_t>(ui::kCanvasHeight - 120.0f));
        const float size = 3.0f + static_cast<float>(next() % 3u);
        const uint32_t kind = next() % 6u;
        const float alpha = 0.25f + static_cast<float>(next() % 56u) / 100.0f;
        if (x > midX - 200.0f && x < midX + 260.0f && y > 240.0f && y < 640.0f) continue;
        if (x > midX - 440.0f && x < midX + 440.0f && y >= 640.0f && y < 840.0f) continue;
        Color col = kind == 0 ? ui::palette::kMarqueeAmber : kind == 1 ? Color::rgb(0xF68CB8) : Color::white(1.0f);
        col.a = alpha;
        if (instead.empty()) c.draw(Rect{x, y, size, size, 0.0f, col});
    }
    const float cabinetW = kCabinetH * 527.0f / 720.0f;
    const TextStyle big = TextStyle::Title2;
    if (instead.empty()) {
        // The bright one is what the eye weighs, so the pair sits a little right of even.
        parts::cabinetDim(c, midX - 5.0f, kTop, kCabinetH);
        parts::cabinet(c, midX - 175.0f, kTop + 28.0f, kCabinetH);
        const std::string first = "Player two,", rest = " from anywhere.";
        const float x = midX - c.measure(first + rest, big) * 0.5f;
        c.text(first, x, 740.0f, big, ui::palette::kMarqueeAmber);
        c.text(rest, x + c.measure(first, big), 740.0f, big, Color::white(0.96f));
        c.centred("Whoever runs your server adds friends", midX, 804.0f, TextStyle::Title3, Color::white(0.60f));
    } else {
        parts::cabinetDim(c, midX - cabinetW * 0.5f, kTop + 14.0f, kCabinetH);
        // What is wrong, and on a second line what to do when the words say.
        const size_t split = instead.find(". ");
        c.centred(split == std::string::npos ? instead : instead.substr(0, split + 1), midX, 740.0f, big, Color::white(0.96f));
        if (split != std::string::npos)
            c.centred(instead.substr(split + 2), midX, 804.0f, TextStyle::Title3, Color::white(0.60f));
    }
}

}  // namespace

ui::Color friendDot(const std::vector<linkup::Friend>& friends, const std::string& name) {
    const std::vector<int> colours = linkup::coloursApart(friends, kFriendColours, 1);
    for (size_t i = 0; i < friends.size(); ++i)
        if (friends[i].name == name) return evenColour(kFriendTops[static_cast<size_t>(colours[i])], 0.74f, 0.13f);
    return ui::palette::kMarqueeAmber;
}

std::vector<int> tileOrder(const std::vector<linkup::Friend>& friends) {
    std::vector<int> order;
    for (const linkup::Group& g : linkup::groups(friends))
        for (int i : g.friends) order.push_back(i);
    return order;
}

void drawBar(ui::Canvas& c, Bar selected, bool focused) {
    parts::topBar(c, {"LinkUp", "Settings"}, static_cast<int>(selected), focused ? 1.0f : 0.0f);
}

void drawFriends(ui::Canvas& c, const FriendsView& v) {
    c.backdrop();
    drawBar(c, Bar::LinkUp, v.barFocused);

    std::vector<Tile> tiles;
    if (v.state.instead.empty()) {
        const std::vector<int> colours = linkup::coloursApart(v.state.friends, kFriendColours, 1);
        for (const linkup::Group& g : linkup::groups(v.state.friends)) {
            for (int i : g.friends) {
                const linkup::Friend& f = v.state.friends[static_cast<size_t>(i)];
                Tile tile;
                tile.kind = g.status;
                tile.name = f.name;
                tile.dark = g.status == linkup::Status::Offline;
                tile.status = linkup::heading(g.status);
                if (g.status == linkup::Status::InvitedYou) tile.status += ": " + f.game;
                else if (linkup::nameOnList(v.state.friends, i) != f.name) tile.status += ", " + f.from;
                const size_t colour = static_cast<size_t>(colours[static_cast<size_t>(i)]);
                tile.top = evenColour(kFriendTops[colour], 0.40f, 0.07f);
                tile.bottom = evenColour(kFriendMids[colour], 0.31f, 0.055f);
                tiles.push_back(std::move(tile));
            }
        }
    }
    if (tiles.empty()) {
        drawNobody(c, v.state.instead);
        return;
    }

    const float usable = ui::kCanvasWidth - parts::kInset * 2.0f;
    const float cellW = (usable - kCellGap * (kColumns - 1)) / kColumns;
    const float pitch = kTileH + kCellGap;
    const float gridTop = parts::kContentTop + parts::pillHeight(c) + kSwitcherGapBelow;
    parts::pill(c, linkup::statusLine(v.state.appearOffline), parts::kInset, parts::kContentTop, true,
                v.focus == -1 && !v.barFocused ? 1.0f : 0.0f);
    const std::string status = linkup::statusLine(v.state.appearOffline);
    c.spot(parts::spot::kStatus, parts::kInset - 14.0f, parts::kContentTop,
           c.measure(status, TextStyle::Title3) + 28.0f, parts::pillHeight(c));

    const int count = static_cast<int>(tiles.size());
    // The focused one last, so it grows over its neighbours.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < count; ++i) {
            const bool on = i == v.focus && !v.barFocused;
            if ((pass == 1) != on) continue;
            const float cellX = parts::kInset + static_cast<float>(i % kColumns) * (cellW + kCellGap);
            const float cellY = gridTop + static_cast<float>(i / kColumns) * pitch;
            c.spot(parts::spot::kItem + i, cellX, cellY, cellW, kTileH);
            const float f = on ? 1.0f : 0.0f;
            const float s = 1.0f + f * (kTileFocusScale - 1.0f);
            const float w = cellW * s, h = kTileH * s;
            drawTile(c, tiles[static_cast<size_t>(i)], cellX - (w - cellW) * 0.5f, cellY - (h - kTileH) * 0.5f, w, h, f);
        }
    }
}

void drawWaiting(ui::Canvas& c, const WaitingView& v) {
    c.backdrop();
    const float midX = ui::kCanvasWidth * 0.5f;
    const float base = ui::kCanvasHeight * 0.5f - 20.0f;
    c.centred(v.title, midX, base, TextStyle::LargeTitle, Color::white(0.96f));
    float y = base + 84.0f;
    if (!v.game.empty()) {
        c.centred(v.game, midX, y, TextStyle::Title3, Color::white(0.60f));
        y += 56.0f;
    }
    parts::waitingDots(c, midX, y, v.clock);
    const float foot = ui::kCanvasHeight - ui::kSafeInset - 130.0f;
    parts::pressPrompt(c, parts::prompt(v.hands, true, "to leave"), midX, foot, 0.80f);
}

void drawPictureStandIn(ui::Canvas& c, const std::string& game, float clock) {
    Rect ground{0, 0, ui::kCanvasWidth, ui::kCanvasHeight, 0, Color::rgb(0x1B3A5C)};
    ground.gradient = true;
    ground.fillBottom = Color::rgb(0x0E1B2C);
    c.draw(ground);
    for (int i = 0; i < 9; ++i) {
        const float t = clock * 0.4f + static_cast<float>(i) * 0.7f;
        c.draw(Rect{160.0f + static_cast<float>(i) * 200.0f, 520.0f + std::sin(t) * 120.0f, 90.0f, 90.0f, 14.0f,
                    Color::white(0.05f)});
    }
    const float midX = ui::kCanvasWidth * 0.5f;
    c.centred(game, midX, 480.0f, TextStyle::LargeTitle, Color::white(0.22f));
    c.centred("The game's picture goes here", midX, 540.0f, TextStyle::Callout, Color::white(0.18f));
}

void drawFirstRun(ui::Canvas& c, const FirstRunView& v) {
    c.backdrop();
    // On the left, what this is: the console's cabinet, a player two behind it, and the line.
    constexpr float kCabinetH = 340.0f;
    constexpr float kLeftMid = 520.0f;
    parts::cabinetDim(c, kLeftMid - 5.0f, 230.0f, kCabinetH);
    parts::cabinet(c, kLeftMid - 175.0f, 258.0f, kCabinetH);
    const TextStyle big = TextStyle::Title2;
    const std::string first = "Player two,", rest = " from anywhere.";
    const float tx = kLeftMid - c.measure(first + rest, big) * 0.5f;
    c.text(first, tx, 720.0f, big, ui::palette::kMarqueeAmber);
    c.text(rest, tx + c.measure(first, big), 720.0f, big, Color::white(0.96f));
    c.centred("Join a friend's game on their CabinetOS console", kLeftMid, 784.0f, TextStyle::Callout, Color::white(0.60f));

    // On the right, the three things a friend sends, and the one thing to press.
    constexpr float kX = 1020.0f, kW = 780.0f, kTop = 290.0f;
    const float pitch = parts::kRowHeight + parts::kRowGap;
    const auto line = [&](int i, const char* label, const std::string& value, const char* hint) {
        const bool on = v.focus == i && !v.connecting;
        parts::Row r;
        r.label = label;
        r.value = value.empty() ? hint : value;
        r.dim = value.empty();
        r.typing = on;
        r.caret = v.caret;
        r.still = v.still;
        r.line = parts::spot::kItem + i;
        const float y = kTop + static_cast<float>(i) * pitch;
        parts::row(c, kX, y, kW, r, on ? 1.0f : 0.0f);
        c.spot(parts::spot::kItem + i, kX, y, kW, parts::kRowHeight);
    };
    line(0, "Server", v.address, "name.example.net:7845");
    line(1, "Code", v.code, "XXXX-XXXX-XXXX");
    line(2, "Username", v.name, "What your friend will see");
    const float buttonY = kTop + 3.0f * pitch + 22.0f;
    if (v.connecting) {
        c.centred("Pairing", kX + kW * 0.5f, buttonY + 50.0f, TextStyle::Title3, Color::white(0.80f));
        parts::waitingDots(c, kX + kW * 0.5f, buttonY + 98.0f, v.clock);
    } else {
        parts::button(c, kX, buttonY, kW, "Connect", v.focus == 3 ? 1.0f : 0.0f);
        c.spot(parts::spot::kItem + 3, kX, buttonY, kW, parts::kRowHeight);
    }
}

const std::vector<SettingsCategory>& settingsCategories() {
    static const std::vector<SettingsCategory> kCategories = {
        {"LinkUp", {setting::Name, setting::Server}},
        {"Controllers", {setting::Controller}},
        {"Display and Sound", {setting::Colour, setting::FullScreen}},
        {"System", {setting::Startup}},
        {"About", {setting::Version, setting::HowTo, setting::Credits}},
    };
    return kCategories;
}

bool settingFocusable(setting::Row row) { return row != setting::Version; }

namespace {

// The console's two columns (design.h): the list at the content inset less half a row's padding,
// 420 wide, each name in Title3 with 16 above and below and 8 between; the rows 60 after it, up to
// 1100 wide, 16 apart (kDetailRowGap).
constexpr float kListX = parts::kInset - 16.0f;
constexpr float kListWidth = 420.0f;
constexpr float kListPadY = 16.0f;
constexpr float kListGap = 8.0f;
constexpr float kPaneGap = 60.0f;
constexpr float kPaneMaxWidth = 1100.0f;
constexpr float kPaneRowGap = 16.0f;
constexpr float kListRadius = 16.0f;
constexpr float kFocusScale = 1.03f;
constexpr float kSelectedTint = 0.30f;
constexpr float kFocusedTint = 0.55f;

parts::Row settingRow(setting::Row id, const SettingsView& v) {
    switch (id) {
        case setting::Name: {
            parts::Row r{"Username", v.name};
            r.typing = v.typingName;
            r.caret = v.caret;
            r.line = parts::spot::kItem + setting::Name;
            return r;
        }
        case setting::Server: {
            parts::Row r{"Server", v.server};
            r.opens = true;
            return r;
        }
        case setting::Controller: {
            const int pads = static_cast<int>(v.controllers.size());
            const int pad = std::clamp(v.controller, 0, std::max(0, pads - 1));
            parts::Row r{"Controller", pads == 0 ? "None connected" : v.controllers[static_cast<size_t>(pad)]};
            r.choice = pads > 1;
            r.canLeft = pad > 0;
            r.canRight = pad + 1 < pads;
            return r;
        }
        case setting::Colour: {
            const int colour = std::clamp(v.colour, 0, look::kColourCount - 1);
            parts::Row r{"Color", look::colourName(static_cast<look::Colour>(colour))};
            r.choice = true;
            r.canLeft = colour > 0;
            r.canRight = colour + 1 < look::kColourCount;
            return r;
        }
        case setting::FullScreen: return {"Full screen", v.fullScreen ? "On" : "Off"};
        case setting::Startup: return {"Startup", v.startup ? "On" : "Off"};
        case setting::Version: return {"Version", v.version};
        case setting::HowTo: {
            parts::Row r{"How to", ""};
            r.opens = true;
            return r;
        }
        case setting::Credits: {
            parts::Row r{"Credits and licenses", ""};
            r.opens = true;
            return r;
        }
        default: return {};
    }
}

}  // namespace

void drawSettings(ui::Canvas& c, const SettingsView& v) {
    c.backdrop();
    drawBar(c, Bar::Settings, v.barFocused);
    const std::vector<SettingsCategory>& cats = settingsCategories();
    const int cat = std::clamp(v.category, 0, static_cast<int>(cats.size()) - 1);

    // The categories. Selection is not focus, and both show at once: the one whose rows are on the
    // right is full white, with a quiet ground under it while its rows have the focus; the one the
    // focus is on has the focused tint and grows a little.
    const float itemH = c.lineHeight(TextStyle::Title3) + kListPadY * 2.0f;
    float y = parts::kContentTop;
    for (int i = 0; i < static_cast<int>(cats.size()); ++i) {
        const bool selected = i == cat;
        const bool focused = selected && v.focus < 0 && !v.barFocused;
        const float s = focused ? kFocusScale : 1.0f;
        const float w = kListWidth * s, h = itemH * s;
        const float x = kListX - (w - kListWidth) * 0.5f, iy = y - (h - itemH) * 0.5f;
        if (focused) c.draw(Rect{x, iy, w, h, kListRadius, Color::white(kFocusedTint)});
        else if (selected && !v.barFocused) c.draw(Rect{x, iy, w, h, kListRadius, Color::white(kSelectedTint * 0.6f)});
        c.text(cats[static_cast<size_t>(i)].name, parts::kInset, iy + kListPadY * s + c.ascent(TextStyle::Title3),
               TextStyle::Title3, Color::white(selected ? 1.0f : 0.60f));
        c.spot(parts::spot::kCategory + i, kListX, y, kListWidth, itemH);
        y += itemH + kListGap;
    }

    // The chosen category's rows.
    const float paneX = kListX + kListWidth + kPaneGap;
    const float paneW = std::min(kPaneMaxWidth, ui::kCanvasWidth - parts::kInset - paneX);
    const std::vector<setting::Row>& rows = cats[static_cast<size_t>(cat)].rows;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const setting::Row id = rows[static_cast<size_t>(i)];
        parts::Row r = settingRow(id, v);
        r.still = v.still;
        const float ry = parts::kContentTop + static_cast<float>(i) * (parts::kRowHeight + kPaneRowGap);
        parts::row(c, paneX, ry, paneW, r, v.focus == i && !v.barFocused ? 1.0f : 0.0f);
        if (settingFocusable(id)) c.spot(parts::spot::kItem + id, paneX, ry, paneW, parts::kRowHeight);
    }
}

// The console's How to (about.cpp): the pause and the leaving, as a friend has them (the friend's
// pause offers Leave where the console's offers Exit to Home), and what the menus need.
struct HowTo {
    const char* what;
    const char* how;
};
constexpr HowTo kHowTo[] = {
    {"Pause menu", "Both sticks in (L3 + R3)"},
    {"Leave a game", "Pause menu, Leave"},
    {"Select", "A"},
    {"Back", "B"},
    {"Change screen", "L1 / R1"},
};

int howToCount() { return static_cast<int>(std::size(kHowTo)); }

void drawHowTo(ui::Canvas& c, const HowToView& v) {
    parts::Panel p;
    p.title = "How to";
    for (const HowTo& line : kHowTo) {
        p.options.push_back(line.what);
        p.values.push_back(line.how);
    }
    p.focused = v.focus;
    p.top = v.top;
    parts::panel(c, p);
}

int creditCount() { return static_cast<int>(std::size(kCredits)); }

void drawCredits(ui::Canvas& c, const CreditsView& v) {
    parts::Panel p;
    p.title = "Credits and licenses";
    for (const Credit& credit : kCredits) {
        p.options.push_back(credit.name);
        p.values.push_back(credit.what);
    }
    p.focused = v.focus;
    p.top = v.top;
    parts::panel(c, p);
}

}  // namespace screens
