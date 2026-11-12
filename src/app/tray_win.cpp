// The window's comings and goings on Windows (tray.h), as Discord, Steam and Teams have them: the X
// hides the window to the tray, whose icon opens it again on a click and opens the menu on a
// right-click.
//
// The icon is the shell's notify icon on a hidden window of the app's own, which SDL's loop serves
// with its own messages; its three pictures, one for each status, are the program's own icons
// (2pstart.rc, from icon/make.swift). The same window answers a second copy started with the same
// folder, which finds the folder's mutex already made and posts it a message to show itself.
//
// An invite is a Windows notification with Join and Decline on it. A program that no package
// installed has no name for the system to show its notifications under, so 2P Start gives itself
// one, under the person's own registry key, each time it starts (as Firefox's own portable copy
// does), so it has one however it came to be on the PC; the Setup's Start menu entry carries the same.
// Should Windows refuse it, the tray's own balloon says the invite instead, and a click on it opens
// Join or Decline.

#include "app/tray.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#include <windowsx.h>

#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

#include "app/tray_system.h"

namespace {

namespace toast = winrt::Windows::UI::Notifications;

constexpr const wchar_t* kClass = L"CabinetOS 2P Start tray";
constexpr const wchar_t* kAppId = L"io.github.mmagtech.2pstart";
constexpr const wchar_t* kAppName = L"2P Start";
constexpr UINT kIconMessage = WM_APP + 1;
constexpr UINT kOpenMessage = WM_APP + 2;
constexpr UINT kIconId = 1;
enum Command : UINT { kOpen = 1, kOnline, kOffline, kStartup, kQuit, kInvite = 100 };

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

// What a notification's XML must not hold as it is.
std::wstring escaped(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        switch (c) {
            case L'&': out += L"&amp;"; break;
            case L'<': out += L"&lt;"; break;
            case L'>': out += L"&gt;"; break;
            case L'"': out += L"&quot;"; break;
            case L'\'': out += L"&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

// The folder the program is in, with a separator after it.
std::wstring programFolder() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0) return {};
        if (n < path.size()) {
            path.resize(n);
            break;
        }
        path.resize(path.size() * 2);
    }
    return path.substr(0, path.find_last_of(L"\\/") + 1);
}

bool setValue(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

// The name and the picture its notifications show, kept under the person's own key.
bool nameForNotifications() {
    const std::wstring path = std::wstring(L"Software\\Classes\\AppUserModelId\\") + kAppId;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return false;
    const bool ok = setValue(key, L"DisplayName", kAppName) &&
                    setValue(key, L"IconUri", programFolder() + L"assets\\2pstart.png");
    RegCloseKey(key);
    return ok;
}

HANDLE heldFolder = nullptr;   // the folder's mutex, held until the program ends

}  // namespace

struct tray::Tray::System {
    uint32_t event = 0;
    HWND window = nullptr;
    UINT taskbarCreated = 0;
    NOTIFYICONDATAW icon{};
    const Asks* asks = nullptr;
    std::vector<Invite> menuInvites;   // the invites the open menu lists, in its order
    std::optional<toast::ToastNotifier> notifier;
    std::map<std::string, toast::ToastNotification> toasts;   // each invite's, by who sent it
    std::mutex mu;
    Reply reply;          // what a click asked for
    std::string shown;    // the friend the last balloon named; empty for "still running"
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

void answer(Tray::System& s, const std::string& who, int answer) {
    {
        std::lock_guard<std::mutex> lock(s.mu);
        s.reply = Reply{who, answer};
    }
    push(s.event, Ask::Reply);
}

void menu(Tray::System& s, POINT at) {
    const Asks& asks = *s.asks;
    s.menuInvites = asks.invites ? asks.invites() : std::vector<Invite>{};
    HMENU m = CreatePopupMenu();
    for (size_t i = 0; i < s.menuInvites.size(); ++i)
        AppendMenuW(m, MF_STRING, kInvite + i, wide(lineOf(s.menuInvites[i])).c_str());
    if (!s.menuInvites.empty()) AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU status = CreatePopupMenu();
    AppendMenuW(status, MF_STRING, kOnline, L"Online");
    AppendMenuW(status, MF_STRING, kOffline, L"Offline");
    const bool online = asks.online && asks.online();
    CheckMenuRadioItem(status, kOnline, kOffline, online ? kOnline : kOffline, MF_BYCOMMAND);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(status), L"Status");
    AppendMenuW(m, MF_STRING, kOpen, L"Open 2P Start");
    const bool canStartup = !asks.canStartup || asks.canStartup();
    AppendMenuW(m, MF_STRING | (asks.startup && asks.startup() ? MF_CHECKED : 0) | (canStartup ? 0 : MF_GRAYED), kStartup,
                L"Startup");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kQuit, L"Quit");
    SetMenuDefaultItem(m, kOpen, FALSE);
    // In front for the menu, so a click anywhere else closes it (Shell_NotifyIcon's own advice).
    SetForegroundWindow(s.window);
    const UINT chosen = static_cast<UINT>(
        TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, at.x, at.y, 0, s.window, nullptr));
    PostMessageW(s.window, WM_NULL, 0, 0);
    DestroyMenu(m);   // and the Status menu inside it
    switch (chosen) {
        case 0: break;
        case kOpen: push(s.event, Ask::Open); break;
        case kOnline: push(s.event, Ask::Online); break;
        case kOffline: push(s.event, Ask::Offline); break;
        case kStartup: push(s.event, Ask::Startup); break;
        case kQuit: {
            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
            break;
        }
        default:
            if (chosen >= kInvite && chosen - kInvite < s.menuInvites.size())
                answer(s, s.menuInvites[chosen - kInvite].who, -1);
            break;
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
                std::string who;
                {
                    std::lock_guard<std::mutex> lock(s->mu);
                    who = s->shown;
                }
                if (who.empty()) push(s->event, Ask::Open);
                else answer(*s, who, -1);
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
    // Asked to end, by Windows signing out or by Setup updating 2P Start (its Restart Manager): it
    // quits, as Quit does, since it lives on in the tray otherwise.
    if (message == WM_ENDSESSION) {
        if (w) {
            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
        }
        return 0;
    }
    // Explorer started again: its tray is new and empty.
    if (message == s->taskbarCreated && s->taskbarCreated != 0) {
        addIcon(*s);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}

// The program's icon for `status` at the tray's size: 1 Offline, 2 Online, 3 an invite (2pstart.rc).
HICON iconFor(Status status) {
    const int id = status == Status::Online ? 2 : status == Status::Invite ? 3 : 1;
    const int size = GetSystemMetrics(SM_CXSMICON);
    // Shared: loaded once, and the same each time it is asked for.
    if (HANDLE own = LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(id), IMAGE_ICON, size, size, LR_SHARED))
        return static_cast<HICON>(own);
    return LoadIconW(nullptr, MAKEINTRESOURCEW(32512));   // IDI_APPLICATION, which is narrow here
}

// The invite as a notification, with Join and Decline on it. It stays until it is answered or the
// invite ends, as a call does: Windows' "reminder". False when Windows would not show it.
bool showToast(Tray::System& s, const Invite& invite) {
    if (!s.notifier) return false;
    try {
        std::wstring xml = L"<toast launch=\"open\" scenario=\"reminder\"><visual><binding template=\"ToastGeneric\">";
        xml += L"<text>" + escaped(wide(invite.title)) + L"</text>";
        if (!invite.game.empty()) xml += L"<text>" + escaped(wide(invite.game)) + L"</text>";
        xml += L"</binding></visual><actions>"
               L"<action content=\"Join\" arguments=\"join\" activationType=\"foreground\"/>"
               L"<action content=\"Decline\" arguments=\"decline\" activationType=\"foreground\"/>"
               L"</actions></toast>";
        winrt::Windows::Data::Xml::Dom::XmlDocument doc;
        doc.LoadXml(xml);
        toast::ToastNotification note(doc);
        Tray::System* sys = &s;
        const std::string who = invite.who;
        // On a thread of Windows' own: the answer goes to the window's thread as an event.
        note.Activated([sys, who](const toast::ToastNotification&, const winrt::Windows::Foundation::IInspectable& args) {
            int a = -1;
            if (auto activated = args.try_as<toast::ToastActivatedEventArgs>()) {
                const std::wstring said(activated.Arguments().c_str());
                if (said == L"join") a = 0;
                else if (said == L"decline") a = 1;
            }
            answer(*sys, who, a);
        });
        auto old = s.toasts.find(who);
        if (old != s.toasts.end()) {
            s.notifier->Hide(old->second);
            s.toasts.erase(old);
        }
        s.notifier->Show(note);
        s.toasts.emplace(who, note);
        return true;
    } catch (const winrt::hresult_error& e) {
        std::fprintf(stderr, "2pstart: no notification: %ls\n", e.message().c_str());
        return false;
    }
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
    s.asks = &asks_;
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
    s.icon.hIcon = iconFor(Status::Offline);
    copyInto(s.icon.szTip, wide(tipOf({}, false)));
    addIcon(s);

    // Notifications: COM on this thread, as SDL may already have it; then the name to show them by.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)com;
    if (nameForNotifications()) {
        try {
            s.notifier = toast::ToastNotificationManager::CreateToastNotifier(kAppId);
        } catch (const winrt::hresult_error& e) {
            std::fprintf(stderr, "2pstart: no notifications: %ls\n", e.message().c_str());
        }
    }
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

void showing(Tray::System&) {}

bool notify(Tray::System& s, const Invite& invite) {
    // Turned off for 2P Start in Windows' settings: no balloon either.
    if (s.notifier) {
        try {
            if (s.notifier->Setting() != toast::NotificationSetting::Enabled) return false;
        } catch (const winrt::hresult_error&) {
        }
    }
    if (showToast(s, invite)) return true;
    {
        std::lock_guard<std::mutex> lock(s.mu);
        s.shown = invite.who;
    }
    // Windows shows the first line bold only when there is a second.
    if (invite.game.empty()) balloon(s, L"", wide(invite.title));
    else balloon(s, wide(invite.title), wide(invite.game));
    return true;
}

void withdraw(Tray::System& s, const std::string& who) {
    auto it = s.toasts.find(who);
    if (it == s.toasts.end() || !s.notifier) return;
    try {
        s.notifier->Hide(it->second);
    } catch (const winrt::hresult_error&) {
    }
    s.toasts.erase(it);
}

void look(Tray::System& s, Status status, const std::string& tip) {
    if (!s.window) return;
    s.icon.hIcon = iconFor(status);
    copyInto(s.icon.szTip, wide(tip));
    NOTIFYICONDATAW d = s.icon;
    d.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_MODIFY, &d);
}

Reply takeReply(Tray::System& s) {
    std::lock_guard<std::mutex> lock(s.mu);
    Reply r;
    std::swap(r, s.reply);
    return r;
}

}  // namespace tray
