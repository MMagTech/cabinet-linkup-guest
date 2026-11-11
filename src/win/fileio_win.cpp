// CabinetOS's fileio.h on Windows. The console's own fileio.cpp is written on POSIX calls; this
// keeps the same promise with Windows' ones: a temporary beside the file, every byte written and
// flushed to the disk, then moved over the old file in one step. A crash or a full disk halfway
// leaves the old file, never an empty or cut one.
//
// Paths are UTF-8, as everywhere in the program, and may use either slash.

#include "fileio.h"

#include <windows.h>

#include <cerrno>
#include <string>

namespace {

std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

// Each folder on the way, made if it is not there. What cannot be made shows when the file is opened.
void makeParents(const std::wstring& path) {
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i] != L'/' && path[i] != L'\\') continue;
        CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
    }
}

}  // namespace

namespace cab {

std::vector<uint8_t> readBytes(const std::string& path) {
    std::vector<uint8_t> out;
    const HANDLE f = CreateFileW(wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return out;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(f, &size) && size.QuadPart > 0) {
        out.resize(static_cast<size_t>(size.QuadPart));
        size_t at = 0;
        while (at < out.size()) {
            const DWORD want = static_cast<DWORD>(out.size() - at > (1u << 30) ? (1u << 30) : out.size() - at);
            DWORD got = 0;
            if (!ReadFile(f, out.data() + at, want, &got, nullptr) || got == 0) break;
            at += got;
        }
        if (at != out.size()) out.clear();
    }
    CloseHandle(f);
    return out;
}

bool writeBytes(const std::string& path, const void* data, size_t size, mode_t) {
    const std::wstring to = wide(path);
    if (to.empty()) return false;
    makeParents(to);
    const std::wstring tmp = to + L".placing";
    DeleteFileW(tmp.c_str());
    const HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    const char* p = static_cast<const char*>(data);
    size_t left = size;
    bool ok = true;
    while (ok && left > 0) {
        const DWORD want = static_cast<DWORD>(left > (1u << 30) ? (1u << 30) : left);
        DWORD wrote = 0;
        if (!WriteFile(f, p, want, &wrote, nullptr) || wrote == 0) {
            ok = false;
            break;
        }
        p += wrote;
        left -= wrote;
    }
    ok = ok && FlushFileBuffers(f) != 0;
    ok = CloseHandle(f) != 0 && ok;
    if (!ok || !MoveFileExW(tmp.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool writeBytes(const std::string& path, const std::vector<uint8_t>& data, mode_t mode) {
    return writeBytes(path, data.data(), data.size(), mode);
}

bool writeBytes(const std::string& path, const std::string& text, mode_t mode) {
    return writeBytes(path, text.data(), text.size(), mode);
}

bool removeFile(const std::string& path) {
    if (DeleteFileW(wide(path).c_str())) return true;
    const DWORD why = GetLastError();
    return why == ERROR_FILE_NOT_FOUND || why == ERROR_PATH_NOT_FOUND;
}

}  // namespace cab

int mkdir(const char* path, mode_t) {
    if (CreateDirectoryW(wide(path).c_str(), nullptr)) return 0;
    errno = GetLastError() == ERROR_ALREADY_EXISTS ? EEXIST : ENOENT;
    return -1;
}

int chmod(const char*, mode_t) { return 0; }
