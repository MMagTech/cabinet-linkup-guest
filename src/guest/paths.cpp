// Where 2P Start keeps what it holds (paths.h).

#include "guest/paths.h"

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>

#include <shlobj.h>
#endif

namespace guest {

std::string dataFolder() {
#ifdef _WIN32
    std::string out;
    PWSTR wide = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wide)) && wide) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if (n > 1) {
            out.resize(static_cast<size_t>(n));
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
            out.resize(static_cast<size_t>(n - 1));   // without the closing zero
        }
    }
    CoTaskMemFree(wide);
    return out.empty() ? out : out + "\\CabinetOS 2P Start";
#else
    const char* home = std::getenv("HOME");
#ifdef __APPLE__
    if (!home || !*home) return {};
    return std::string(home) + "/Library/Application Support/CabinetOS 2P Start";
#else
    const char* data = std::getenv("XDG_DATA_HOME");
    if (data && *data) return std::string(data) + "/cabinetos-2p-start";
    if (!home || !*home) return {};
    return std::string(home) + "/.local/share/cabinetos-2p-start";
#endif
#endif
}

}  // namespace guest
