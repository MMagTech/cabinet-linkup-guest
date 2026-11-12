// A line's right-click menu on Windows (menu.h): a popup menu, as an edit box's, with each item's
// Ctrl key beside it.

#include "app/menu.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace menu {

Edit edit(SDL_Window* window, const Can& can) {
    const auto hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) return Edit::None;
    HMENU m = CreatePopupMenu();
    if (!m) return Edit::None;
    // Each item's id is its Edit; 0, None, is what TrackPopupMenu returns for nothing chosen.
    const auto add = [m](const wchar_t* words, Edit e, bool on) {
        AppendMenuW(m, MF_STRING | (on ? MF_ENABLED : MF_GRAYED), static_cast<UINT_PTR>(e), words);
    };
    add(L"Cu&t\tCtrl+X", Edit::Cut, can.cut);
    add(L"&Copy\tCtrl+C", Edit::Copy, can.copy);
    add(L"&Paste\tCtrl+V", Edit::Paste, can.paste);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(L"Select &All\tCtrl+A", Edit::SelectAll, can.selectAll);
    POINT at{};
    GetCursorPos(&at);
    const UINT chosen = static_cast<UINT>(
        TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, at.x, at.y, 0, hwnd, nullptr));
    DestroyMenu(m);
    return static_cast<Edit>(chosen);
}

}  // namespace menu
