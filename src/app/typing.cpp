// Typing in a line, in 2P Start's window (app.h): the caret, the chosen letters, Cut, Copy, Paste and
// Undo, and a line's right-click menu.

#include "app/app.h"

#include "app/shared.h"

namespace {

// The system's own key for Cut, Copy, Paste, Select All and Undo: Command on a Mac, Ctrl elsewhere.
bool command(SDL_Keymod mod) {
#if defined(SDL_PLATFORM_MACOS)
    return (mod & SDL_KMOD_GUI) != 0;
#else
    return (mod & SDL_KMOD_CTRL) != 0;
#endif
}

}  // namespace

using namespace shared;

bool App::typing() const {
    if (overlay_.kind == Overlay::Kind::Field) return true;
    if (overlay_.kind != Overlay::Kind::None) return false;
    if (place_ == Place::FirstRun) return first_.focus < 3 && !first_.connecting;
    return place_ == Place::Settings && typingName_;
}

std::string* App::typedLine(textline::Kind& kind) {
    if (overlay_.kind == Overlay::Kind::Field) {
        kind = overlay_.fieldKind;
        return &overlay_.field.value;
    }
    if (place_ == Place::Settings && typingName_) {
        kind = textline::Kind::Name;
        return &typedName_;
    }
    if (place_ == Place::FirstRun) {
        switch (first_.focus) {
            case 0: kind = textline::Kind::Address; return &first_.address;
            case 1: kind = textline::Kind::Code; return &first_.code;
            case 2: kind = textline::Kind::Name; return &first_.name;
            default: break;
        }
    }
    return nullptr;
}

// A key or a move holds the caret still, as on the console: it blinks only while it waits.
void App::edit(const std::function<textline::Line(textline::Line, textline::Kind)>& how, Undo::Change change) {
    textline::Kind kind;
    std::string* line = typedLine(kind);
    if (!line) return;
    const textline::Line was{*line, caret_, anchor_};
    textline::Line now = how(was, kind);
    if (now.text != was.text) undo_.before(was, change);
    else undo_.moved();
    *line = std::move(now.text);
    caret_ = now.caret;
    anchor_ = now.anchor;
    still_ = 0.0f;
}

void App::typeText(const std::string& text, Undo::Change change) {
    edit([&text](textline::Line l, textline::Kind kind) { return textline::add(std::move(l), text, kind); }, change);
}

// Left and Right are moves (move), so a held one repeats as every held direction does. These keys
// repeat as the computer repeats them. With Shift, Home and End choose as they go.
bool App::typeKey(const SDL_KeyboardEvent& key) {
    using textline::Kind;
    using textline::Line;
    const bool choosing = (key.mod & SDL_KMOD_SHIFT) != 0;
    const auto to = [this, choosing](size_t at) {
        edit([at, choosing](Line l, Kind kind) { return textline::moveTo(std::move(l), at, kind, choosing); });
    };
    switch (key.key) {
        case SDLK_BACKSPACE:
            edit([](Line l, Kind kind) { return textline::backspace(std::move(l), kind); }, Undo::Change::Erased);
            return true;
        case SDLK_DELETE:
            edit([](Line l, Kind kind) { return textline::forwardDelete(std::move(l), kind); }, Undo::Change::Erased);
            return true;
        case SDLK_HOME: to(0); return true;
        case SDLK_END: to(std::string::npos); return true;
#if defined(SDL_PLATFORM_MACOS)
        // A Mac's Home and End.
        case SDLK_LEFT:
        case SDLK_RIGHT:
            if (!(key.mod & SDL_KMOD_GUI)) return false;
            to(key.key == SDLK_LEFT ? 0 : std::string::npos);
            return true;
#endif
        case SDLK_A:
        case SDLK_C:
        case SDLK_X:
        case SDLK_V:
        case SDLK_Z:
            if (!command(key.mod)) return false;
            switch (key.key) {
                case SDLK_A: apply(menu::Edit::SelectAll); break;
                case SDLK_C: apply(menu::Edit::Copy); break;
                case SDLK_X: apply(menu::Edit::Cut); break;
                case SDLK_V: apply(menu::Edit::Paste); break;
                // Shift with it is Redo elsewhere, which this has not: it does nothing.
                default: if (!choosing) undo(); break;
            }
            return true;
        default: return false;
    }
}

void App::apply(menu::Edit e) {
    using textline::Kind;
    using textline::Line;
    textline::Kind kind;
    const std::string* line = typedLine(kind);
    if (!line) return;
    const std::string chosen = textline::chosenText(Line{*line, caret_, anchor_}, kind);
    switch (e) {
        case menu::Edit::None: return;
        case menu::Edit::Cut:
        case menu::Edit::Copy:
            if (chosen.empty()) return;
            SDL_SetClipboardText(chosen.c_str());
            if (e == menu::Edit::Cut) edit([](Line l, Kind k) { return textline::backspace(std::move(l), k); });
            return;
        case menu::Edit::Paste:
            if (char* clip = SDL_GetClipboardText()) {
                typeText(clip, Undo::Change::Other);
                SDL_free(clip);
            }
            return;
        case menu::Edit::SelectAll: edit([](Line l, Kind k) { return textline::all(std::move(l), k); }); return;
    }
}

void App::undo() {
    textline::Kind kind;
    std::string* line = typedLine(kind);
    if (!line) return;
    const std::optional<textline::Line> was = undo_.back();
    if (!was) return;
    *line = was->text;
    caret_ = was->caret;
    anchor_ = was->anchor;
    still_ = 0.0f;
}

parts::Span App::chosenSpan() {
    textline::Kind kind;
    const std::string* line = typedLine(kind);
    if (!line) return {};
    const auto [from, to] = textline::chosen(textline::Line{*line, caret_, anchor_}, kind);
    return {from, to};
}

void App::caretToEnd() {
    textline::Kind kind;
    const std::string* line = typedLine(kind);
    caret_ = line ? line->size() : 0;
    anchor_.reset();
    undo_.clear();
}

void App::clickCaret(int spot, float x, bool fresh, int clicks) {
    using textline::Kind;
    using textline::Line;
    if (fresh) caretToEnd();
    dragging_ = spot;
    if (clicks >= 3) {
        edit([](Line l, Kind kind) { return textline::all(std::move(l), kind); });
        return;
    }
    const std::optional<size_t> at = canvas_.caretAt(spot, x);
    if (!at) return;
    if (clicks == 2) {
        edit([at](Line l, Kind kind) { return textline::word(std::move(l), *at, kind); });
        return;
    }
    const bool choosing = shiftHeld();
    edit([at, choosing](Line l, Kind kind) { return textline::moveTo(std::move(l), *at, kind, choosing); });
}

void App::dragTo(float x) {
    if (!typing()) return;
    float cx = 0, cy = 0;
    canvas_.toCanvas(x, 0.0f, cx, cy);
    const std::optional<size_t> at = canvas_.caretAt(dragging_, cx, true);
    if (at) edit([at](textline::Line l, textline::Kind kind) { return textline::moveTo(std::move(l), *at, kind, true); });
}

bool App::isLine(int spot) const {
    namespace s = parts::spot;
    if (overlay_.kind == Overlay::Kind::Field) return spot == s::kField;
    if (overlay_.kind != Overlay::Kind::None) return false;
    switch (place_) {
        case Place::FirstRun: return !first_.connecting && spot >= s::kItem && spot < s::kItem + 3;
        case Place::Settings: return spot == s::kItem + screens::setting::Name;
        case Place::Friends:
        case Place::Game: return false;
    }
    return false;
}

// As a text box's: the right-click puts the caret where it is, as a click would, unless it is on the
// chosen letters, which then stay chosen for Cut or Copy. A line not being typed into is pressed into
// typing first, so the Code line can be pasted into with the mouse alone.
bool App::lineMenu(float x, float y) {
    ui::Spot s;
    if (!spotAt(x, y, s, overlay_.kind != Overlay::Kind::None) || !isLine(s.id)) return false;
    setHands(parts::Hands::Mouse);
    float cx = 0, cy = 0;
    canvas_.toCanvas(x, y, cx, cy);
    const parts::Span was = chosenSpan();
    const std::optional<size_t> at = canvas_.caretAt(s.id, cx);
    const bool onChosen = typing() && !was.empty() && at && *at >= was.from && *at <= was.to;
    if (!onChosen) pointAt(x, y, 1);
    dragging_ = 0;
    textline::Kind kind;
    const std::string* line = typedLine(kind);
    if (!typing() || !line) return true;
    menu::Can can;
    can.cut = can.copy = !chosenSpan().empty();
    can.paste = SDL_HasClipboardText();
    can.selectAll = !line->empty();
    apply(menu::edit(window_, can));
    pointerUnheard_ = true;
    return true;
}
