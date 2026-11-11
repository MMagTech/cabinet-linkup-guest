// Where focus goes (moves.h).

#include "app/moves.h"

#include <algorithm>

namespace moves {

Step friends(int focus, int tiles, Move m) {
    using Then = Step::Then;
    auto go = [](int to) { return Step{to, Then::Moved}; };
    const Step edge{focus, Then::Edge};
    if (m == Move::Back) return {focus, Then::Back};
    if (tiles <= 0) return m == Move::Up ? Step{focus, Then::ToBar} : edge;
    if (focus < 0) {
        if (m == Move::Up) return {focus, Then::ToBar};
        if (m == Move::Press) return {focus, Then::Press};
        if (m == Move::Down) return go(0);
        return edge;
    }
    focus = std::min(focus, tiles - 1);
    switch (m) {
        case Move::Up: return go(focus < kColumns ? -1 : focus - kColumns);
        case Move::Down:
            if (focus / kColumns == (tiles - 1) / kColumns) return edge;
            return go(std::min(focus + kColumns, tiles - 1));
        case Move::Left:
            if (focus % kColumns == 0) return edge;
            return go(focus - 1);
        case Move::Right:
            if (focus % kColumns == kColumns - 1 || focus + 1 >= tiles) return edge;
            return go(focus + 1);
        case Move::Press: return {focus, Then::Press};
        default: return edge;
    }
}

Step column(int focus, int count, Move m, bool bar) {
    using Then = Step::Then;
    const Step edge{focus, Then::Edge};
    if (m == Move::Back) return {focus, Then::Back};
    if (count <= 0) return m == Move::Up && bar ? Step{focus, Then::ToBar} : edge;
    focus = std::clamp(focus, 0, count - 1);
    switch (m) {
        case Move::Up:
            if (focus > 0) return {focus - 1, Then::Moved};
            return bar ? Step{focus, Then::ToBar} : edge;
        case Move::Down:
            if (focus + 1 < count) return {focus + 1, Then::Moved};
            return edge;
        case Move::Press: return {focus, Then::Press};
        default: return edge;
    }
}

BarStep bar(int at, int count, Move m, bool screenTakesFocus) {
    BarStep s;
    s.at = at;
    switch (m) {
        case Move::Left:
        case Move::Right:
        case Move::PrevScreen:
        case Move::NextScreen: {
            const int d = m == Move::Right || m == Move::NextScreen ? 1 : -1;
            const int next = at + d;
            if (next < 0 || next >= count) s.edge = true;
            else s.at = next;
            return s;
        }
        case Move::Down:
        case Move::Press:
        case Move::Back:
            // Neither leaves for a screen with nothing to focus: focus would go where nobody can see it.
            if (screenTakesFocus) s.leave = true;
            else s.edge = true;
            return s;
        case Move::Up:
            s.edge = true;
            return s;
    }
    return s;
}

int topFor(int focus, int top, int count, int rows) {
    const int most = std::max(0, count - rows);
    if (focus < top) top = focus;
    else if (focus >= top + rows) top = focus - rows + 1;
    return std::clamp(top, 0, most);
}

}  // namespace moves
