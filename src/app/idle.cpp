// Whether anybody is at the computer (idle.h): the rule, then each computer's way of saying.
//
// macOS: the seconds since the last key or mouse from the HID system, the screen's lock from the
// session, a dark screen from the display, and sleep from IOKit's power messages. The power message
// is answered a moment late, so the status can go first: macOS waits up to 30 seconds for an answer.
//
// Windows: the seconds since the last input (GetLastInputInfo), the lock from the session
// (WTSQuerySessionInformation), and sleep and a dark screen from the power manager's callbacks, which
// come on a thread of their own. The sleep callback holds the computer a second before it returns, so
// the status can go first: Windows allows about two.

#include "app/idle.h"

#include <algorithm>
#include <atomic>
#include <cstring>

#if defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <powrprof.h>
#include <wtsapi32.h>
#endif

namespace idle {

bool Rule::away(const Seen& seen, double sincePress) {
    double idle = sincePress;
    if (seen.since >= 0.0) idle = std::min(idle, seen.since);
    // A touch: the seconds since the last one went down since the last look.
    const bool touch = lastIdle_ >= 0.0 && idle < lastIdle_;
    lastIdle_ = idle;
    if (seen.off) {
        // Going off is not a touch: the press that locked the screen came before it. Typing the
        // password back in is.
        if (!held_) touched_ = false;
        else if (touch) touched_ = true;
        held_ = true;
        return true;
    }
    if (held_ && (touched_ || touch)) held_ = false;
    return held_ || idle > kAwaySeconds;
}

namespace {

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point t) { return std::chrono::duration<double>(Clock::now() - t).count(); }

}  // namespace

#if defined(__APPLE__)

struct Watch::System {
    std::function<void()> wake;
    io_connect_t root = MACH_PORT_NULL;
    IONotificationPortRef port = nullptr;
    io_object_t notifier = 0;
    bool sleeping = false;
    // The sleep to answer, once the status has had its moment.
    bool asked = false;
    intptr_t question = 0;
    Clock::time_point askedAt;
};

namespace {

// On the window's thread: IOKit's port is on its run loop, which SDL runs while it waits for events.
void onPower(void* context, io_service_t, natural_t message, void* argument) {
    auto* s = static_cast<Watch::System*>(context);
    switch (message) {
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(s->root, reinterpret_cast<intptr_t>(argument));
            break;
        case kIOMessageSystemWillSleep:
            s->sleeping = true;
            s->asked = true;
            s->question = reinterpret_cast<intptr_t>(argument);
            s->askedAt = Clock::now();
            if (s->wake) s->wake();
            break;
        case kIOMessageSystemWillPowerOn:
        case kIOMessageSystemHasPoweredOn:
            s->sleeping = false;
            break;
        default:
            break;
    }
}

void listen(Watch::System& s) {
    s.root = IORegisterForSystemPower(&s, &s.port, onPower, &s.notifier);
    if (s.root == MACH_PORT_NULL) return;
    CFRunLoopAddSource(CFRunLoopGetMain(), IONotificationPortGetRunLoopSource(s.port), kCFRunLoopCommonModes);
}

void stopListening(Watch::System& s) {
    if (s.root == MACH_PORT_NULL) return;
    if (s.asked) IOAllowPowerChange(s.root, s.question);
    CFRunLoopRemoveSource(CFRunLoopGetMain(), IONotificationPortGetRunLoopSource(s.port), kCFRunLoopCommonModes);
    IODeregisterForSystemPower(&s.notifier);
    IOServiceClose(s.root);
    IONotificationPortDestroy(s.port);
}

bool locked() {
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (!session) return false;
    const void* value = CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked"));
    const bool on = value && CFGetTypeID(value) == CFBooleanGetTypeID() && CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    CFRelease(session);
    return on;
}

Seen look(Watch::System& s) {
    // The status has had its moment: the computer may sleep.
    if (s.asked && secondsSince(s.askedAt) >= 0.5) {
        IOAllowPowerChange(s.root, s.question);
        s.asked = false;
    }
    Seen seen;
    seen.since = CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGAnyInputEventType);
    seen.off = s.sleeping || locked() || CGDisplayIsAsleep(CGMainDisplayID());
    return seen;
}

bool urgent(const Watch::System& s) { return s.asked; }

}  // namespace

#elif defined(_WIN32)

struct Watch::System {
    std::function<void()> wake;
    std::atomic<bool> sleeping{false};
    std::atomic<bool> dark{false};
    std::atomic<bool> news{false};
    DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS params{};
    HPOWERNOTIFY suspend = nullptr;
    HPOWERNOTIFY display = nullptr;
};

namespace {

// GUID_CONSOLE_DISPLAY_STATE, spelled out so nothing has to define it.
const GUID kDisplayState = {0x6fe69556, 0x704a, 0x47a0, {0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47}};

// On the power manager's thread.
ULONG CALLBACK onPower(PVOID context, ULONG type, PVOID setting) {
    auto* s = static_cast<Watch::System*>(context);
    if (type == PBT_APMSUSPEND) {
        s->sleeping = true;
        s->news = true;
        if (s->wake) s->wake();
        Sleep(1000);
    } else if (type == PBT_APMRESUMEAUTOMATIC || type == PBT_APMRESUMESUSPEND) {
        s->sleeping = false;
    } else if (type == PBT_POWERSETTINGCHANGE && setting) {
        const auto* p = static_cast<const POWERBROADCAST_SETTING*>(setting);
        if (IsEqualGUID(p->PowerSetting, kDisplayState) && p->DataLength >= sizeof(DWORD)) {
            DWORD state = 0;
            memcpy(&state, p->Data, sizeof state);
            const bool dark = state == 0;   // 1 is on, 2 is dimmed
            if (dark != s->dark.exchange(dark) && dark) {
                s->news = true;
                if (s->wake) s->wake();
            }
        }
    }
    return 0;
}

void listen(Watch::System& s) {
    s.params.Callback = onPower;
    s.params.Context = &s;
    PowerRegisterSuspendResumeNotification(DEVICE_NOTIFY_CALLBACK, &s.params, &s.suspend);
    PowerSettingRegisterNotification(&kDisplayState, DEVICE_NOTIFY_CALLBACK, &s.params, &s.display);
}

void stopListening(Watch::System& s) {
    if (s.suspend) PowerUnregisterSuspendResumeNotification(s.suspend);
    if (s.display) PowerSettingUnregisterNotification(s.display);
}

bool locked() {
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &buffer, &bytes))
        return false;
    bool on = false;
    if (buffer && bytes >= sizeof(WTSINFOEXW)) {
        const auto* info = reinterpret_cast<const WTSINFOEXW*>(buffer);
        on = info->Level == 1 && info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK;
    }
    WTSFreeMemory(buffer);
    return on;
}

Seen look(Watch::System& s) {
    s.news = false;
    Seen seen;
    LASTINPUTINFO last{};
    last.cbSize = sizeof last;
    if (GetLastInputInfo(&last)) seen.since = static_cast<double>(GetTickCount() - last.dwTime) / 1000.0;
    seen.off = s.sleeping || s.dark || locked();
    return seen;
}

bool urgent(const Watch::System& s) { return s.news; }

}  // namespace

#else

// Elsewhere the computer says nothing here: only the app's own presses count.
struct Watch::System {
    std::function<void()> wake;
};

namespace {

void listen(Watch::System&) {}
void stopListening(Watch::System&) {}
Seen look(Watch::System&) { return {}; }
bool urgent(const Watch::System&) { return false; }

}  // namespace

#endif

Watch::Watch(std::function<void()> wake) : system_(std::make_unique<System>()), lastPress_(Clock::now()) {
    system_->wake = std::move(wake);
    listen(*system_);
}

Watch::~Watch() { stopListening(*system_); }

void Watch::pressed() { lastPress_ = Clock::now(); }

bool Watch::away() {
    if (looked_ && !urgent(*system_) && secondsSince(lastLook_) < 1.0) return away_;
    looked_ = true;
    lastLook_ = Clock::now();
    away_ = rule_.away(look(*system_), secondsSince(lastPress_));
    return away_;
}

}  // namespace idle
