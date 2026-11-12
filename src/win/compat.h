// Put in front of every file on Windows (CMakeLists.txt), so that CabinetOS's LinkUp code builds
// there as it is written: the few POSIX names it uses that Microsoft's compiler does not have.

#pragma once

#if defined(_WIN32) && defined(__cplusplus)

#include <sys/types.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>

#ifdef _MSC_VER
using mode_t = unsigned short;
#endif

// keys.cpp makes its folder with these two. A folder under a person's own AppData is already
// theirs alone, so the mode is not used. Both are in fileio_win.cpp.
int mkdir(const char* path, mode_t mode);
int chmod(const char* path, mode_t mode);

// rtc.cpp's clock, which only goes forward: Windows' own steady one.
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
inline int clock_gettime(int, timespec* ts) {
    const long long ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    ts->tv_sec = static_cast<time_t>(ns / 1000000000);
    ts->tv_nsec = static_cast<long>(ns % 1000000000);
    return 0;
}

#endif
