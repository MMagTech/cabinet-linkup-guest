// 2P Start's own parts held to account, with no network: files written whole on this system (on
// Windows that is this repository's own code, src/win/fileio_win.cpp), the console's key store
// working through them, the account id staying the same, and what a username may be.
//
//   guest-test

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "fileio.h"
#include "guest/account.h"
#include "guest/paths.h"
#include "keys.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    ++failures;
    std::fprintf(stderr, "FAILED: %s\n", what.c_str());
}

std::string textOf(const std::vector<uint8_t>& bytes) { return std::string(bytes.begin(), bytes.end()); }

// A path as the program holds it, UTF-8, for the standard library to look at.
std::filesystem::path seen(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

void filesAreWrittenWhole(const std::string& root) {
    // A folder that is not there yet, two deep, with a name beyond ASCII.
    const std::string path = root + "/made/Zo\xc3\xab/file";
    check(cab::readBytes(path).empty(), "a file that is not there read as something");
    check(cab::writeBytes(path, std::string("first, and longer")), "a file in new folders was not written");
    check(textOf(cab::readBytes(path)) == "first, and longer", "a file did not read back as written");
    check(std::filesystem::exists(seen(path)), "the file is not where its UTF-8 name says");

    check(cab::writeBytes(path, std::string("second")), "a file was not replaced");
    check(textOf(cab::readBytes(path)) == "second", "a replaced file kept some of the old one");
    check(!std::filesystem::exists(seen(path + ".placing")), "the temporary was left beside the file");

    std::vector<uint8_t> all(256);
    for (size_t i = 0; i < all.size(); ++i) all[i] = static_cast<uint8_t>(i);
    check(cab::writeBytes(path, all) && cab::readBytes(path) == all, "every byte value did not survive");

    check(cab::removeFile(path) && !std::filesystem::exists(seen(path)), "a file was not removed");
    check(cab::removeFile(path), "removing a file that is gone was not a success");
    std::printf("  files are written whole\n");
}

void theKeyStoreHolds(const std::string& root) {
    const std::string folder = root + "/keys";
    noise::Key mine{};
    int64_t lastT = 0;
    {
        Keys keys(folder);
        check(keys.load(), "a new folder could not hold a key");
        mine = keys.mine().pub;
        check(mine != noise::Key{}, "the new key is all zeros");
        check(!keys.server(), "a new folder already remembers a server");
        Keys::Server server;
        server.address = "linkup.example.org:7845";
        server.name = "Riley's";
        server.key.fill(7);
        check(keys.remember(server), "a server was not remembered");
        const int64_t a = keys.nextT();
        lastT = keys.nextT();
        check(lastT > a, "t did not go up");
    }
    {
        Keys keys(folder);
        check(keys.load() && keys.mine().pub == mine, "the key changed between two runs");
        const auto server = keys.server();
        check(server && server->address == "linkup.example.org:7845" && server->name == "Riley's" &&
                  server->key[0] == 7,
              "the remembered server did not come back");
        check(keys.nextT() > lastT, "t went back between two runs");
        check(keys.forget(), "the server was not forgotten");
    }
    {
        Keys keys(folder);
        check(keys.load() && !keys.server() && keys.mine().pub == mine, "forgetting the server did not keep the key alone");
    }
    std::printf("  the console's key store holds\n");
}

void theAccountStays(const std::string& root) {
    const std::string folder = root + "/account-a";
    check(guest::accountId(folder).empty(), "an account id was there before pairing");
    const std::string first = guest::accountFor(folder, "K7MR-4QXT-9HPV");
    check(first.size() == 32, "an account id is not 32 characters");
    check(first.find_first_not_of("0123456789abcdef") == std::string::npos, "an account id is not hex");
    check(guest::accountId(folder).empty(), "an account id was kept before pairing worked");
    check(guest::accountFor(root + "/account-b", "k7mr 4qxt 9hpv") == first,
          "the same code, typed another way on another computer, gave another id");
    check(guest::accountFor(folder, "K7MR-4QXT-9HPW") != first, "two codes gave the same id");
    check(guest::accountFor(folder, "K7MR-4QXT").empty(), "a code too short gave an id");

    check(guest::keepAccount(folder, first) && guest::accountId(folder) == first, "the id was not kept");
    check(guest::accountFor(folder, "AAAA-BBBB-CCCC") == first, "a changed code changed the kept id");
    check(guest::keepAccount(folder, guest::accountFor(root + "/account-c", "AAAA-BBBB-CCCC")) &&
              guest::accountId(folder) == first,
          "pairing again replaced the kept id");
    check(!guest::keepAccount(root + "/account-d", "not an id") && guest::accountId(root + "/account-d").empty(),
          "something not an id was kept");

    check(guest::username(folder).empty(), "a name was there before one was given");
    check(guest::keepUsername(folder, "alex") && guest::username(folder) == "alex", "a name was not kept");
    check(!guest::keepUsername(folder, "") && guest::username(folder) == "alex", "an empty name replaced a real one");
    std::printf("  the account stays the same\n");
}

void usernamesFit() {
    check(guest::usernameFits("alex"), "a plain name was refused");
    check(guest::usernameFits("Zo\xc3\xab"), "a name beyond ASCII was refused");
    check(guest::usernameFits(std::string(128, 'a')), "128 bytes were refused");
    check(!guest::usernameFits(""), "an empty name was taken");
    check(!guest::usernameFits(std::string(129, 'a')), "129 bytes were taken");
    check(!guest::usernameFits("a\nb"), "a name with a line break was taken");
    check(!guest::usernameFits("a\x1b[31m"), "a name with an escape was taken");
    check(!guest::usernameFits("a\xff"), "broken UTF-8 was taken");
    std::printf("  what a username may be\n");
}

void theFolderHasAPlace() {
    const std::string folder = guest::dataFolder();
    check(!folder.empty(), "this computer named no folder");
    check(folder.find("2P Start") != std::string::npos || folder.find("2p-start") != std::string::npos,
          "the folder does not carry the app's name");
    std::printf("  the app's folder has a place\n");
}

}  // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("2pstart-test-" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::u8string u8 = root.generic_u8string();
    const std::string rootText(u8.begin(), u8.end());

    filesAreWrittenWhole(rootText);
    theKeyStoreHolds(rootText);
    theAccountStays(rootText);
    usernamesFit();
    theFolderHasAPlace();

    std::filesystem::remove_all(root, ignored);
    if (failures > 0) {
        std::fprintf(stderr, "%d failed\n", failures);
        return 1;
    }
    std::printf("guest-test: all passed\n");
    return 0;
}
