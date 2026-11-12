// The parts more than one screen draws (parts.h).

#include "ui/parts.h"

#include <algorithm>
#include <cmath>

namespace parts {

using ui::Color;
using ui::Rect;
using ui::TextStyle;

namespace {

constexpr float kPillPadX = 14.0f;
constexpr float kPillPadY = 8.0f;
constexpr float kPillFocusScale = 1.06f;
constexpr float kFocusedTint = 0.55f;
constexpr float kSelectedTint = 0.30f;

// The panel a menu sits on (panels.h, menuPanel).
Rect menuPanel(Color fill, float x, float y, float w, float h) {
    fill.a = 0.92f;
    Rect p{x, y, w, h, 32.0f, fill};
    p.gradient = true;
    p.fillBottom = Color{fill.r * 0.62f, fill.g * 0.62f, fill.b * 0.62f, 0.96f};
    p.border = 1.5f;
    p.borderColor = Color::white(0.14f);
    p.shadowBlur = 64.0f;
    p.shadowOffsetY = 22.0f;
    p.shadowColor = Color::black(0.60f);
    return p;
}

// A button on such a panel, at rest and under focus (panels.h, menuButton).
Rect menuButton(float x, float y, float w, float h, float radius, float f) {
    Rect b{x, y, w, h, radius, Color::white(0.06f + f * 0.10f)};
    if (f > 0.0f) {
        b.border = f * kFocusRim;
        b.borderColor = ui::palette::kFocusRim;
        b.shadowBlur = 22.0f;
        b.shadowOffsetY = 6.0f;
        b.shadowColor = Color::black(0.45f * f);
    }
    return b;
}

Color menuLabel(float f) { return Color::white(0.62f + f * 0.38f); }

// The one arrow the console uses for "this opens something", and its mirror for a choice's left.
const char* const kChevron = "\xE2\x80\xBA";
const char* const kChevronBack = "\xE2\x80\xB9";

// A long title on two lines, broken after a space or a hyphen, the second cut if it must be
// (screenparts.cpp, wrapTwoLines).
std::vector<std::string> twoLines(ui::Canvas& c, const std::string& s, TextStyle st, float width) {
    if (c.measure(s, st) <= width) return {s};
    size_t brk = std::string::npos;
    std::string line;
    for (size_t i = 1; i <= s.size(); ++i) {
        const bool afterSpace = i < s.size() && s[i] == ' ';
        const bool afterHyphen = s[i - 1] == '-';
        if (!afterSpace && !afterHyphen) continue;
        const std::string candidate = s.substr(0, i);
        if (c.measure(candidate, st) > width) break;
        brk = afterSpace ? i + 1 : i;
        line = candidate;
    }
    if (brk == std::string::npos || brk >= s.size()) return {c.truncate(s, st, width)};
    return {line, c.truncate(s.substr(brk), st, width)};
}

const char* const kEllipsis = "\xE2\x80\xA6";

// A whole letter at a time, never into the middle of one (keyboard.cpp, nextChar).
size_t nextLetter(const std::string& s, size_t at) {
    if (at >= s.size()) return s.size();
    ++at;
    while (at < s.size() && (static_cast<unsigned char>(s[at]) & 0xC0) == 0x80) ++at;
    return at;
}

// `at` on the edge of a letter, back to its start if it is inside one, and no further than the end.
size_t onEdge(const std::string& s, size_t at) {
    at = std::min(at, s.size());
    while (at > 0 && at < s.size() && (static_cast<unsigned char>(s[at]) & 0xC0) == 0x80) --at;
    return at;
}

// What of a typed line is drawn: the bytes from `from` to `to`, with an ellipsis for what came off
// either end.
struct Shown {
    std::string words;
    size_t from = 0, to = 0;
    bool cutStart = false;
};

// What of a typed line fits `width`, with the caret at `caret` in view, as the console's keyboard
// keeps its field (keyboard.cpp): all of it when it fits; otherwise letters come off the start
// while the caret is past them, so the end shows while somebody types there, and then off the end.
Shown shownOf(ui::Canvas& c, const std::string& s, size_t caret, TextStyle st, float width) {
    std::vector<size_t> edges{0};
    for (size_t i = 0; i < s.size();) {
        i = nextLetter(s, i);
        edges.push_back(i);
    }
    const size_t n = edges.size() - 1;
    size_t at = n;   // the caret, counted in letters
    for (size_t k = 0; k <= n; ++k) {
        if (edges[k] >= caret) {
            at = k;
            break;
        }
    }
    size_t first = 0, last = n;
    const auto words = [&]() {
        return std::string(first > 0 ? kEllipsis : "") + s.substr(edges[first], edges[last] - edges[first]) +
               (last < n ? kEllipsis : "");
    };
    std::string w = words();
    while (last - first > 1 && c.measure(w, st) > width) {
        if (first < at) ++first;
        else --last;
        w = words();
    }
    return {w, edges[first], edges[last], first > 0};
}

// Where, from `x`, the gap before byte `at` of the line was drawn.
float gapX(ui::Canvas& c, const std::string& s, const Shown& sh, size_t at, float x, TextStyle st) {
    return x + c.measure(std::string(sh.cutStart ? kEllipsis : "") + s.substr(sh.from, at - sh.from), st);
}

// Each gap between the letters drawn, kept under the line's spot for a click (Canvas::letters).
void keepLetters(ui::Canvas& c, int id, float left, float right, const std::string& s, const Shown& sh, float x,
                 TextStyle st) {
    ui::Letters l;
    l.id = id;
    l.left = left;
    l.right = right;
    for (size_t at = sh.from;; at = nextLetter(s, at)) {
        l.gaps.push_back({gapX(c, s, sh, at, x, st), at});
        if (at >= sh.to) break;
    }
    c.letters(std::move(l));
}

}  // namespace

void topBar(ui::Canvas& c, const std::vector<std::string>& labels, int selected, float focus) {
    constexpr float kItemGap = 60.0f, kPadX = 22.0f, kInsetY = 6.0f;
    const TextStyle st = TextStyle::Callout;
    const float baseline = kBarTop + (kBarHeight - c.lineHeight(st)) * 0.5f + c.ascent(st);
    float x = kInset;
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        const std::string& label = labels[static_cast<size_t>(i)];
        const float w = c.measure(label, st);
        const bool on = i == selected;
        if (on) {
            const float h = kBarHeight - kInsetY * 2.0f;
            const float tint = kSelectedTint + focus * (kFocusedTint - kSelectedTint);
            c.draw(Rect{x - kPadX, kBarTop + kInsetY, w + kPadX * 2.0f, h, h * 0.5f, Color::white(tint * 0.36f)});
        }
        c.text(label, x, baseline, st, Color::white(on ? 1.0f : 0.65f));
        c.spot(spot::kBar + i, x - kPadX, kBarTop, w + kPadX * 2.0f, kBarHeight);
        x += w + kItemGap;
    }
}

float pillHeight(ui::Canvas& c) { return c.lineHeight(TextStyle::Title3) + kPillPadY * 2.0f; }

void pill(ui::Canvas& c, const std::string& label, float x, float y, bool selected, float focus) {
    const TextStyle st = TextStyle::Title3;
    const float w0 = c.measure(label, st) + kPillPadX * 2.0f;
    const float h0 = pillHeight(c);
    const float s = 1.0f + focus * (kPillFocusScale - 1.0f);
    const float w = w0 * s, h = h0 * s;
    // `x` is where the words start. The capsule grows round them and does not push them along.
    const float px = x - kPillPadX - (w - w0) * 0.5f;
    const float py = y - (h - h0) * 0.5f;
    if (focus > 0.001f) c.draw(Rect{px, py, w, h, h * 0.5f, Color::white(focus * kFocusedTint * 0.5f)});
    c.text(label, x, py + kPillPadY * s + c.ascent(st), st, Color::white(selected ? 1.0f : 0.45f));
}

Prompt prompt(Hands hands, bool back, const std::string& after) {
    switch (hands) {
        case Hands::Keyboard: return {"Press", back ? "Esc" : "Enter", after};
        case Hands::Mouse: return {back ? "Right-click" : "Click", "", after};
        case Hands::Controller: break;
    }
    return {"Press", back ? "B" : "A", after};
}

void pressPrompt(ui::Canvas& c, const Prompt& p, float midX, float baseline, float alpha) {
    const TextStyle st = TextStyle::Title3;
    const float gap = 16.0f;
    if (p.button.empty()) {
        const std::string words = p.before + " " + p.after;
        c.centred(words, midX, baseline, st, Color::white(0.92f * alpha));
        return;
    }
    // A letter sits in a disc; a key's name, smaller, in a capsule the same height.
    const bool letter = p.button.size() == 1;
    const TextStyle inner = letter ? st : TextStyle::Callout;
    const float badgeH = c.lineHeight(st) * 0.95f;
    const float badgeW = letter ? badgeH : c.measure(p.button, inner) + 36.0f;
    const float w1 = c.measure(p.before, st), w2 = c.measure(p.after, st);
    float x = midX - (w1 + gap + badgeW + gap + w2) * 0.5f;
    c.text(p.before, x, baseline, st, Color::white(0.92f * alpha));
    x += w1 + gap;
    const float capMid = baseline - c.ascent(st) * 0.36f;
    c.draw(Rect{x, capMid - badgeH * 0.5f, badgeW, badgeH, badgeH * 0.5f, Color::white(0.95f * alpha)});
    Color ink = ui::palette::kInk;
    ink.a = alpha;
    const float innerBase = letter ? baseline : capMid + c.capHeight(inner) * 0.5f;
    c.text(p.button, x + (badgeW - c.measure(p.button, inner)) * 0.5f, innerBase, inner, ink);
    x += badgeW + gap;
    c.text(p.after, x, baseline, st, Color::white(0.92f * alpha));
}

void notice(ui::Canvas& c, const std::string& words, Tone tone, const ui::Color* dot) {
    constexpr float kPillH = 64.0f, kPad = 30.0f, kDot = 14.0f, kGap = 16.0f;
    const TextStyle st = TextStyle::Callout;
    const float w = kPad + kDot + kGap + c.measure(words, st) + kPad;
    const float x = (ui::kCanvasWidth - w) * 0.5f;
    const float y = ui::kCanvasHeight - ui::kSafeInset - kPillH - 24.0f;
    Color fill = c.surface();
    fill.a = 0.96f;
    Rect body{x, y, w, kPillH, kPillH * 0.5f, fill};
    body.border = 1.5f;
    body.borderColor = Color::white(0.14f);
    body.shadowBlur = 26.0f;
    body.shadowOffsetY = 10.0f;
    body.shadowColor = Color::black(0.45f);
    c.draw(body);
    const Color kind = dot                    ? *dot
                       : tone == Tone::Done    ? ui::palette::kScreenCyan
                       : tone == Tone::Problem ? ui::palette::kMarqueeAmber
                                               : Color::white(0.70f);
    c.draw(Rect{x + kPad, y + (kPillH - kDot) * 0.5f, kDot, kDot, kDot * 0.5f, kind});
    c.text(words, x + kPad + kDot + kGap, y + (kPillH - c.lineHeight(st)) * 0.5f + c.ascent(st), st, Color::white(0.94f));
}

void panel(ui::Canvas& c, const Panel& p) {
    constexpr float kPad = 40.0f, kMaxW = 1200.0f, kButtonW = 600.0f, kButtonH = 88.0f, kButtonGap = 12.0f,
                    kButtonRadius = 16.0f, kFocusScale = 1.04f;
    const float W = ui::kCanvasWidth, H = ui::kCanvasHeight;
    const bool prompt = p.options.empty() && !p.prompt.before.empty();
    const bool list = !p.values.empty();
    const int n = static_cast<int>(p.options.size());
    // A list grows to its longest line, name and value, up to the panel's limit. A question keeps
    // its fixed buttons.
    float rowW = kButtonW;
    if (list) {
        for (int i = 0; i < n; ++i) {
            const std::string& value = i < static_cast<int>(p.values.size()) ? p.values[static_cast<size_t>(i)] : "";
            const float vw = value.empty() ? 0.0f : c.measure(value, TextStyle::Callout) + 24.0f;
            rowW = std::max(rowW, c.measure(p.options[static_cast<size_t>(i)], TextStyle::Title3) + vw + 48.0f);
        }
        rowW = std::min(rowW, kMaxW - kPad * 2.0f);
    }
    float inner = std::max(rowW, c.measure(p.title, TextStyle::Title2));
    if (!p.detail.empty()) inner = std::max(inner, c.measure(p.detail, TextStyle::Callout));
    if (p.width > 0.0f) rowW = inner = std::min(p.width, kMaxW - kPad * 2.0f);
    const float panelW = std::min(kMaxW, inner + kPad * 2.0f);
    const float textMax = panelW - kPad * 2.0f;
    const std::vector<std::string> titleLines =
        p.width > 0.0f ? twoLines(c, p.title, TextStyle::Title2, textMax)
                       : std::vector<std::string>{c.truncate(p.title, TextStyle::Title2, textMax)};
    const float titleLine = c.lineHeight(TextStyle::Title2);
    const float titleH = titleLine * static_cast<float>(titleLines.size());
    const float detailH = p.detail.empty() ? 0.0f : 8.0f + c.lineHeight(TextStyle::Callout);
    const int shown = std::min(n, kPanelRows);
    const float listH = prompt ? kButtonH
                        : shown == 0 ? 0.0f
                                     : static_cast<float>(shown) * kButtonH + static_cast<float>(shown - 1) * kButtonGap;
    const float below = listH > 0.0f ? 36.0f + listH : 0.0f;
    const float panelH = kPad + titleH + detailH + below + kPad;
    const float px = (W - panelW) * 0.5f, py = (H - panelH) * 0.5f;

    c.dim(0.55f);
    c.draw(menuPanel(c.surface(), px, py, panelW, panelH));
    c.spot(spot::kPanel, px, py, panelW, panelH);

    float y = py + kPad;
    float base = y + c.ascent(TextStyle::Title2);
    for (const std::string& line : titleLines) {
        c.centred(line, W * 0.5f, base, TextStyle::Title2, Color::white(1.0f));
        base += titleLine;
    }
    y += titleH;
    if (!p.detail.empty())
        c.centred(c.truncate(p.detail, TextStyle::Callout, textMax), W * 0.5f, y + 8.0f + c.ascent(TextStyle::Callout),
                  TextStyle::Callout, Color::white(0.60f));
    y += detailH + 36.0f;

    if (prompt) {
        pressPrompt(c, p.prompt, W * 0.5f, y + kButtonH * 0.5f + c.ascent(TextStyle::Title3) * 0.40f, 1.0f);
        return;
    }
    const TextStyle st = TextStyle::Title3;
    const int first = std::clamp(p.top, 0, std::max(0, n - kPanelRows));
    // The focused one last, so it grows over its neighbours.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = first; i < std::min(n, first + kPanelRows); ++i) {
            const bool on = i == p.focused;
            if ((pass == 1) != on) continue;
            const float f = on ? 1.0f : 0.0f;
            const float s = 1.0f + f * (kFocusScale - 1.0f);
            const float bw = rowW * s, bh = kButtonH * s;
            const float bx = (W - bw) * 0.5f;
            const float restY = y + static_cast<float>(i - first) * (kButtonH + kButtonGap);
            const float by = restY - (bh - kButtonH) * 0.5f;
            // Where it rests, not where focus grows it, so it is no harder to leave than to enter.
            c.spot(spot::kOption + i, (W - rowW) * 0.5f, restY, rowW, kButtonH);
            c.draw(menuButton(bx, by, bw, bh, kButtonRadius, f));
            const float textBase = by + (bh - c.lineHeight(st)) * 0.5f + c.ascent(st);
            const std::string& label = p.options[static_cast<size_t>(i)];
            if (!list) {
                c.centred(label, W * 0.5f, textBase, st, menuLabel(f));
                continue;
            }
            // A list reads left to right, "name ... value", like a Settings row.
            const std::string value = i < static_cast<int>(p.values.size()) ? p.values[static_cast<size_t>(i)] : "";
            const float vw = value.empty() ? 0.0f : c.measure(value, TextStyle::Callout);
            const float room = bw - 48.0f - (value.empty() ? 0.0f : vw + 24.0f);
            c.text(c.truncate(label, st, room), bx + 24.0f, textBase, st, menuLabel(f));
            if (!value.empty())
                c.text(value, bx + bw - 24.0f - vw,
                       by + (bh - c.lineHeight(TextStyle::Callout)) * 0.5f + c.ascent(TextStyle::Callout),
                       TextStyle::Callout, Color::white(0.60f));
        }
    }
}

void row(ui::Canvas& c, float x, float y, float w, const Row& r, float focus) {
    const float s = 1.0f + focus * 0.02f;
    const float bw = w * s, bh = kRowHeight * s;
    const float bx = x - (bw - w) * 0.5f, by = y - (bh - kRowHeight) * 0.5f;
    // A greyed row keeps a fainter surface, so the list still reads as one, and its words at the
    // console's kSettingsUnbuiltAlpha.
    const float a = r.greyed ? 0.38f : 1.0f;
    if (r.greyed) c.draw(Rect{bx, by, bw, bh, 18.0f, Color::white(0.03f)});
    else c.draw(menuButton(bx, by, bw, bh, 18.0f, focus));
    const TextStyle st = TextStyle::Title3, vs = TextStyle::Callout;
    constexpr float kPad = 32.0f, kArrowGap = 14.0f, kArrowOn = 0.70f, kArrowOff = 0.18f;
    const float labelBase = y + (kRowHeight - c.lineHeight(st)) * 0.5f + c.ascent(st);
    const float valueBase = y + (kRowHeight - c.lineHeight(vs)) * 0.5f + c.ascent(vs);
    c.text(r.label, x + kPad, labelBase, st, Color::white(menuLabel(focus).a * a));

    float right = x + w - kPad;
    // Something that opens: the one arrow the console uses for it (settings.cpp).
    if (r.opens) {
        const float cw = c.measure(kChevron, st);
        right -= cw;
        c.text(kChevron, right, labelBase, st, Color::white(0.30f));
        right -= 16.0f;
    }
    const bool arrows = r.choice && focus > 0.0f;
    if (arrows) {
        const float aw = c.measure(kChevron, vs);
        right -= aw;
        c.text(kChevron, right, valueBase, vs, Color::white((r.canRight ? kArrowOn : kArrowOff) * focus));
        right -= kArrowGap;
    }
    // The caret, with room kept for it after the last letter.
    constexpr float kCaretRoom = 9.0f;
    if (r.typing) right -= kCaretRoom;
    const float valueLeft = x + kPad + c.measure(r.label, st) + kPad;
    const float room = right - valueLeft;
    // A line typed into keeps its caret in view, at its start when it is not being typed into.
    // Anything else keeps its start. As on the console, a line with nothing typed shows its
    // placeholder and no caret.
    const bool typed = (r.typing || r.line != 0) && !r.dim;
    const size_t at = onEdge(r.value, r.typing ? r.caret : 0);
    Shown sh;
    if (typed) sh = shownOf(c, r.value, at, vs, room);
    else sh.words = c.truncate(r.value, vs, room);
    const float shownW = c.measure(sh.words, vs);
    const float textX = right - shownW;
    c.text(sh.words, textX, valueBase, vs, Color::white((r.dim ? 0.35f : r.typing ? 0.94f : 0.60f + focus * 0.34f) * a));
    if (typed && r.line != 0) keepLetters(c, r.line, valueLeft, x + w, r.value, sh, textX, vs);
    // After the last letter, a gap after it; inside the line, on the gap between two.
    if (typed && r.typing)
        caret(c, at >= r.value.size() ? right + kCaretRoom * 0.5f : gapX(c, r.value, sh, at, textX, vs), valueBase, vs,
              r.still);
    if (arrows) {
        const float aw = c.measure(kChevronBack, vs);
        c.text(kChevronBack, right - shownW - kArrowGap - aw, valueBase, vs,
               Color::white((r.canLeft ? kArrowOn : kArrowOff) * focus));
    }
}

void caret(ui::Canvas& c, float x, float baseline, TextStyle st, float still) {
    constexpr float kCaretW = 3.0f, kOver = 5.0f, kUnder = 8.0f, kBlink = 1.0f;
    const float t = std::fmod(std::max(still, 0.0f), kBlink);
    const float half = kBlink * 0.5f, fade = kBlink * 0.1f;
    float on = 1.0f;
    if (t >= kBlink - fade) on = (t - (kBlink - fade)) / fade;
    else if (t >= half + fade) on = 0.0f;
    else if (t >= half) on = 1.0f - (t - half) / fade;
    if (on <= 0.0f) return;
    Color cyan = ui::palette::kScreenCyan;
    cyan.a *= on;
    const float capH = c.capHeight(st);
    c.draw(Rect{x - kCaretW * 0.5f, baseline - capH - kOver, kCaretW, capH + kOver + kUnder, 1.5f, cyan});
}

void fieldPanel(ui::Canvas& c, const FieldPanel& p) {
    // The keyboard's panel and field (keyboard.cpp), as wide as a long address needs.
    constexpr float kPad = 40.0f, kW = 1000.0f, kTitleH = 70.0f, kHintH = 34.0f, kFieldH = 96.0f,
                    kFieldRadius = 16.0f, kGap = 28.0f, kPromptH = 88.0f;
    const float W = ui::kCanvasWidth, H = ui::kCanvasHeight;
    const float hintH = p.hint.empty() ? 0.0f : kHintH;
    const float panelH = kPad + kTitleH + hintH + kFieldH + kGap + kPromptH + kPad;
    const float px = (W - kW) * 0.5f, py = (H - panelH) * 0.5f;
    // The console blurs what is behind its keyboard. With no blur here, it is the panel every other
    // question is on, so nothing behind shows through.
    c.dim(0.55f);
    c.draw(menuPanel(c.surface(), px, py, kW, panelH));
    c.spot(spot::kPanel, px, py, kW, panelH);
    float y = py + kPad;
    c.text(p.title, px + kPad, y + c.ascent(TextStyle::Title2), TextStyle::Title2, Color::white(1.0f));
    if (!p.hint.empty())
        c.text(p.hint, px + kPad, y + kTitleH + c.ascent(TextStyle::Callout) - 10.0f, TextStyle::Callout,
               Color::white(0.55f));
    y += kTitleH + hintH;

    // Focus is in the field: the rim, inside its edge.
    const float fieldW = kW - kPad * 2.0f;
    Rect field{px + kPad, y, fieldW, kFieldH, kFieldRadius, Color::black(0.45f)};
    field.border = kFocusRim;
    field.borderColor = ui::palette::kFocusRim;
    c.draw(field);
    c.spot(spot::kField, field.x, field.y, field.w, field.h);
    const TextStyle st = TextStyle::Title3;
    const float base = y + kFieldH * 0.5f + c.ascent(st) * 0.5f;
    const float textX = px + kPad + 20.0f;
    // White is what was typed, grey everything else. A long line keeps its caret in view.
    const bool empty = p.value.empty();
    const size_t at = onEdge(p.value, p.caret);
    Shown sh;
    if (empty) sh.words = p.placeholder;
    else sh = shownOf(c, p.value, at, st, fieldW - 56.0f);
    c.text(sh.words, textX, base, st, empty ? Color::white(0.35f) : Color::white(1.0f));
    if (!empty) {
        keepLetters(c, spot::kField, field.x, field.x + field.w, p.value, sh, textX, st);
        caret(c, gapX(c, p.value, sh, at, textX, st), base, st, p.still);
    }
    y += kFieldH + kGap;
    pressPrompt(c, p.prompt, W * 0.5f, y + kPromptH * 0.5f + c.ascent(st) * 0.40f, 1.0f);
}

void button(ui::Canvas& c, float x, float y, float w, const std::string& label, float focus) {
    const float s = 1.0f + focus * 0.02f;
    const float bw = w * s, bh = kRowHeight * s;
    c.draw(menuButton(x - (bw - w) * 0.5f, y - (bh - kRowHeight) * 0.5f, bw, bh, 18.0f, focus));
    const TextStyle st = TextStyle::Title3;
    c.centred(label, x + w * 0.5f, y + (kRowHeight - c.lineHeight(st)) * 0.5f + c.ascent(st), st, menuLabel(focus));
}

void cabinet(ui::Canvas& c, float x, float y, float h) {
    // The cabinet spans x 248 to 775 and y 130 to 850 of its own 1024 space (startscreen.cpp).
    const float s = h / 720.0f;
    const float ox = x - 248.0f * s, oy = y - 130.0f * s;
    const auto R = [&](float rx, float ry, float rw, float rh, float rad, Color col) {
        c.draw(Rect{ox + rx * s, oy + ry * s, rw * s, rh * s, rad * s, col});
    };
    const auto dot = [&](float cx, float cy, float rad, Color col) { R(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, rad, col); };
    const Color body = Color::rgb(0xF2EEE8), deck = Color::rgb(0xD7D1C8);

    R(268, 130, 488, 670, 42, body);
    // The marquee, pink to amber, left to right.
    Rect marquee{ox + 318 * s, oy + 176 * s, 390 * s, 84 * s, 16 * s, Color::rgb(0xFF96CA)};
    marquee.gradient = true;
    marquee.across = true;
    marquee.fillBottom = Color::rgb(0xFFCB6E);
    c.draw(marquee);
    // The screen, amber down to red.
    Rect screen{ox + 316 * s, oy + 292 * s, 394 * s, 284 * s, 18 * s, Color::rgb(0xFEE08F)};
    screen.gradient = true;
    screen.fillBottom = Color::rgb(0xEB5736);
    c.draw(screen);

    R(248, 600, 527, 112, 20, deck);
    dot(348, 656, 30, Color::rgb(0x3A3444));
    dot(500, 656, 25, Color::rgb(0xEC405C));
    dot(583, 656, 24, Color::rgb(0xFFC457));
    dot(692, 656, 54, Color::rgb(0xFFB27A, 0.10f));
    dot(692, 656, 40, Color::rgb(0xFFB27A, 0.16f));
    dot(692, 656, 28, Color::rgb(0xFFB27A, 0.22f));
    R(668, 646, 48, 20, 10, Color::rgb(0xFFF0E2));
    R(303, 795, 419, 55, 14, deck);
}

void cabinetDim(ui::Canvas& c, float x, float y, float h) {
    // It is stacked shapes, and made see-through they show through each other. So it is drawn
    // whole and the background comes over it, only there.
    cabinet(c, x, y, h);
    constexpr float kMargin = 24.0f;
    const float w = h * 527.0f / 720.0f;
    c.setClip(x - kMargin, y - kMargin, w + kMargin * 2.0f, h + kMargin * 2.0f);
    c.backdrop(0.68f);
    c.clearClip();
}

void waitingDots(ui::Canvas& c, float midX, float y, float clock) {
    constexpr float kDot = 16.0f, kDotGap = 22.0f;
    const float x0 = midX - (kDot * 3.0f + kDotGap * 2.0f) * 0.5f;
    for (int i = 0; i < 3; ++i) {
        const float phase = clock * 3.0f - static_cast<float>(i) * 0.7f;
        const float alpha = 0.25f + 0.65f * (0.5f + 0.5f * std::sin(phase));
        c.draw(Rect{x0 + static_cast<float>(i) * (kDot + kDotGap), y - kDot, kDot, kDot, kDot * 0.5f, Color::white(alpha)});
    }
}

}  // namespace parts
