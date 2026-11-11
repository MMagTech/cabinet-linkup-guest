// Put in front of every file on Windows (CMakeLists.txt), so that CabinetOS's LinkUp code builds
// there as it is written: the few POSIX names it uses that Microsoft's compiler does not have.

#pragma once

#if defined(_WIN32) && defined(__cplusplus)

#include <sys/types.h>

#include <cctype>
#include <cstdlib>

#ifdef _MSC_VER
using mode_t = unsigned short;
#endif

// keys.cpp makes its folder with these two. A folder under a person's own AppData is already
// theirs alone, so the mode is not used. Both are in fileio_win.cpp.
int mkdir(const char* path, mode_t mode);
int chmod(const char* path, mode_t mode);

#endif
