// The window's comings and goings on Windows (tray.h), as Discord, Steam and Teams have them: the X
// hides the window to the tray, whose icon opens it again on a click and has Open, Status and Quit
// on a right-click.
//
// The icon is the shell's notify icon on a hidden window of the app's own, which SDL's loop serves
// with its own messages. Its balloons are the system's notifications on Windows 10 and 11, and a
// click on one comes back to that window. The same window answers a second copy started with the
// same folder, which finds the folder's mutex already made and posts it a message to show itself.

#include "app/tray.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#include <windowsx.h>

#include <mutex>

#include "app/tray_system.h"

namespace {

constexpr const wchar_t* kClass = L"CabinetOS 2P Start tray";
constexpr const wchar_t* kTip = L"CabinetOS 2P Start";
constexpr UINT kIconMessage = WM_APP + 1;
constexpr UINT kOpenMessage = WM_APP + 2;
constexpr UINT kIconId = 1;
enum Command : UINT { kOpen = 1, kStatus, kQuit };

std::wstring wide(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    out.resize(static_cast<size_t>(n - 1));
    return out;
}

template <size_t N>
void copyInto(wchar_t (&to)[N], const std::wstring& from) {
    wcsncpy_s(to, N, from.c_str(), _TRUNCATE);
}

HANDLE heldFolder = nullptr;   // the folder's mutex, held until the program ends

}  // namespace

struct tray::Tray::System {
    uint32_t event = 0;
    HWND window = nullptr;
    UINT taskbarCreated = 0;
    NOTIFYICONDATAW icon{};
    std::function<std::string()> statusLine;
    std::mutex mu;
    std::string answer;   // the friend a clicked notification named
    std::string shown;    // the friend the last notification named; empty for "still running"
};

namespace tray {

namespace {

void addIcon(Tray::System& s) {
    Shell_NotifyIconW(NIM_ADD, &s.icon);
    NOTIFYICONDATAW v = s.icon;
    v.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &v);
}

void balloon(Tray::System& s, const std::wstring& title, const std::wstring& line) {
    NOTIFYICONDATAW d = s.icon;
    d.uFlags = NIF_INFO;
    copyInto(d.szInfoTitle, title);
    copyInto(d.szInfo, line);
    d.dwInfoFlags = NIIF_NONE;
    Shell_NotifyIconW(NIM_MODIFY, &d);
}

void menu(Tray::System& s, POINT at) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, kOpen, L"Open");
    AppendMenuW(m, MF_STRING, kStatus, wide(s.statusLine ? s.statusLine() : std::string("Status")).c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kQuit, L"Quit");
    SetMenuDefaultItem(m, kOpen, FALSE);
    // In front for the menu, so a click anywhere else closes it (Shell_NotifyIcon's own advice).
    SetForegroundWindow(s.window);
    const UINT chosen = static_cast<UINT>(
        TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, at.x, at.y, 0, s.window, nullptr));
    PostMessageW(s.window, WM_NULL, 0, 0);
    DestroyMenu(m);
    switch (chosen) {
        case kOpen: push(s.event, Ask::Open); break;
        case kStatus: push(s.event, Ask::Status); break;
        case kQuit: {
            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
            break;
        }
        default: break;
    }
}

LRESULT CALLBACK onMessage(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<Tray::System*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!s) return DefWindowProcW(window, message, w, l);
    if (message == kIconMessage) {
        switch (LOWORD(l)) {
            case NIN_SELECT:
            case NIN_KEYSELECT: push(s->event, Ask::Open); break;
            case WM_CONTEXTMENU: menu(*s, POINT{GET_X_LPARAM(w), GET_Y_LPARAM(w)}); break;
            case NIN_BALLOONUSERCLICK: {
                std::lock_guard<std::mutex> lock(s->mu);
                s->answer = s->shown;
                push(s->event, s->shown.empty() ? Ask::Open : Ask::Answer);
                break;
            }
            default: break;
        }
        return 0;
    }
    if (message == kOpenMessage) {
        push(s->event, Ask::Open);
        return 0;
    }
    // Explorer started again: its tray is new and empty.
    if (message == s->taskbarCreated && s->taskbarCreated != 0) {
        addIcon(*s);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

HICON appIcon() {
    // The program's own icon once it has one (piece F); until then the system's for any program.
    if (HICON own = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1))) return own;
    return LoadIconW(nullptr, MAKEINTRESOURCEW(32512));   // IDI_APPLICATION, which is narrow here
}

}  // namespace

bool another(const std::string& folder, bool quietly) {
    const std::wstring key = wide(folderKey(folder));
    HANDLE m = CreateMutexW(nullptr, FALSE, (L"Local\\CabinetOS 2P Start " + key).c_str());
    if (!m) return false;
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        heldFolder = m;
        return false;
    }
    CloseHandle(m);
    if (!quietly) {
        if (HWND first = FindWindowW(kClass, key.c_str())) {
            DWORD pid = 0;
            GetWindowThreadProcessId(first, &pid);
            // The first copy may come to the front: it is what this one was started for.
            AllowSetForegroundWindow(pid);
            PostMessageW(first, kOpenMessage, 0, 0);
        }
    }
    return true;
}

Tray::Tray(SDL_Window* window, const std::string& folder, Asks asks)
    : window_(window), asks_(std::move(asks)), system_(std::make_unique<System>()) {
    System& s = *system_;
    s.event = SDL_RegisterEvents(1);
    s.statusLine = asks_.statusLine;
    s.taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW c{};
    c.cbSize = sizeof c;
    c.lpfnWndProc = onMessage;
    c.hInstance = instance;
    c.lpszClassName = kClass;
    RegisterClassExW(&c);
    // Hidden, never shown: a window only for the tray's messages. Not a message-only window, which
    // would not hear that Explorer started again.
    s.window = CreateWindowExW(0, kClass, wide(folderKey(folder)).c_str(), WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr,
                               instance, nullptr);
    if (!s.window) return;
    SetWindowLongPtrW(s.window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&s));
    s.icon.cbSize = sizeof s.icon;
    s.icon.hWnd = s.window;
    s.icon.uID = kIconId;
    s.icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    s.icon.uCallbackMessage = kIconMessage;
    s.icon.hIcon = appIcon();
    copyInto(s.icon.szTip, kTip);
    addIcon(s);
}

Tray::~Tray() {
    System& s = *system_;
    if (!s.window) return;
    NOTIFYICONDATAW d = s.icon;
    d.uFlags = 0;
    Shell_NotifyIconW(NIM_DELETE, &d);
    SetWindowLongPtrW(s.window, GWLP_USERDATA, 0);
    DestroyWindow(s.window);
}

uint32_t eventOf(const Tray::System& s) { return s.event; }

// The first time, one notification says the X did not close it.
void hide(Tray::System& s, SDL_Window* window, const Asks& asks) {
    SDL_HideWindow(window);
    if (asks.sayStillRunning && asks.sayStillRunning()) {
        {
            std::lock_guard<std::mutex> lock(s.mu);
            s.shown.clear();
        }
        balloon(s, L"", L"Still running in the tray");
    }
}

void notify(Tray::System& s, const std::string& who, const std::string& title, const std::string& game) {
    {
        std::lock_guard<std::mutex> lock(s.mu);
        s.shown = who;
    }
    // Windows shows the first line bold only when there is a second.
    if (game.empty()) balloon(s, L"", wide(title));
    else balloon(s, wide(title), wide(game));
}

std::string takeAnswer(Tray::System& s) {
    std::lock_guard<std::mutex> lock(s.mu);
    std::string who;
    who.swap(s.answer);
    return who;
}

}  // namespace tray
