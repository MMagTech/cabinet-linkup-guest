// Startup (startup.h): the computer's entry that opens the app at login.

#include "app/startup.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#if defined(__APPLE__)
#include <climits>
#include <mach-o/dyld.h>
#include <vector>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace startup {

namespace {

// What a property list's <string> must not hold as it is.
std::string escaped(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c; break;
        }
    }
    return out;
}

#if defined(__APPLE__)

std::string program() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size + 1, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
    char real[PATH_MAX];
    return realpath(path.data(), real) ? std::string(real) : std::string(path.data());
}

#elif defined(_WIN32)

constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunName = L"CabinetOS 2P Start";

std::wstring wide(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    out.resize(static_cast<size_t>(n - 1));
    return out;
}

std::string narrow(const std::wstring& s) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.resize(static_cast<size_t>(n - 1));
    return out;
}

std::string program() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0) return {};
        if (n < path.size()) {
            path.resize(n);
            return narrow(path);
        }
        path.resize(path.size() * 2);
    }
}

#endif

}  // namespace

std::string agent(const std::string& program) {
    return std::string(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
               "<plist version=\"1.0\">\n"
               "<dict>\n"
               "\t<key>Label</key>\n"
               "\t<string>") +
           kLabel +
           "</string>\n"
           "\t<key>ProgramArguments</key>\n"
           "\t<array>\n"
           "\t\t<string>" +
           escaped(program) +
           "</string>\n"
           "\t\t<string>" +
           kFlag +
           "</string>\n"
           "\t</array>\n"
           // Once, at login, in the person's own session with its screen: never again by itself.
           "\t<key>RunAtLoad</key>\n"
           "\t<true/>\n"
           "\t<key>LimitLoadToSessionType</key>\n"
           "\t<string>Aqua</string>\n"
           "\t<key>ProcessType</key>\n"
           "\t<string>Interactive</string>\n"
           "</dict>\n"
           "</plist>\n";
}

bool setAgent(const std::string& folder, const std::string& program, bool on, std::string& why) {
    namespace fs = std::filesystem;
    const fs::path file = fs::path(folder) / (std::string(kLabel) + ".plist");
    std::error_code e;
    if (!on) {
        fs::remove(file, e);
        if (e) why = "could not remove " + file.string() + ": " + e.message();
        return !e;
    }
    if (program.empty()) {
        why = "this program cannot say where it is";
        return false;
    }
    fs::create_directories(folder, e);
    const fs::path part = file.string() + ".part";
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out << agent(program);
        if (!out) {
            why = "could not write " + part.string();
            return false;
        }
    }
    fs::rename(part, file, e);
    if (e) why = "could not write " + file.string() + ": " + e.message();
    return !e;
}

std::string runValue(const std::string& program) { return "\"" + program + "\" " + kFlag; }

bool set(bool on, std::string& why) {
#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        why = "no home folder";
        return false;
    }
    return setAgent(std::string(home) + "/Library/LaunchAgents", on ? program() : std::string(), on, why);
#elif defined(_WIN32)
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        why = "the Run key cannot be opened";
        return false;
    }
    LSTATUS done = ERROR_SUCCESS;
    if (on) {
        const std::string path = program();
        if (path.empty()) {
            RegCloseKey(key);
            why = "this program cannot say where it is";
            return false;
        }
        const std::wstring value = wide(runValue(path));
        done = RegSetValueExW(key, kRunName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                              static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        done = RegDeleteValueW(key, kRunName);
        if (done == ERROR_FILE_NOT_FOUND) done = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (done != ERROR_SUCCESS) why = on ? "the Run value cannot be set" : "the Run value cannot be removed";
    return done == ERROR_SUCCESS;
#else
    (void)on;
    why = "Startup is for macOS and Windows";
    return false;
#endif
}

}  // namespace startup
