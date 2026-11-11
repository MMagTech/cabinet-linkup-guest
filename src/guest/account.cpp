// Who this friend is to a LinkUp server (account.h).

#include "guest/account.h"

#include <array>
#include <cstdint>
#include <vector>

#include <openssl/rand.h>

#include "box.h"
#include "fileio.h"
#include "hexutil.h"

namespace guest {

namespace {

constexpr size_t kIdBytes = 16;

}  // namespace

std::string accountId(const std::string& folder) {
    const std::string path = folder + "/account";
    const std::vector<uint8_t> held = cab::readBytes(path);
    std::array<uint8_t, kIdBytes> raw{};
    const std::string kept(held.begin(), held.end());
    if (hexutil::decode(kept, raw)) return kept;

    if (RAND_bytes(raw.data(), static_cast<int>(raw.size())) != 1) return {};
    const std::string made = hexutil::encode(raw.data(), raw.size());
    if (!cab::writeBytes(path, made, 0600)) return {};
    return made;
}

std::string username(const std::string& folder) {
    const std::vector<uint8_t> held = cab::readBytes(folder + "/username");
    const std::string name(held.begin(), held.end());
    return usernameFits(name) ? name : std::string();
}

bool keepUsername(const std::string& folder, const std::string& name) {
    return usernameFits(name) && cab::writeBytes(folder + "/username", name, 0600);
}

bool usernameFits(const std::string& name) {
    // The console's own cleaning takes out what a server refuses. A name it leaves alone fits.
    return !name.empty() && name.size() <= box::kMaxUsername && box::cleanText(name, box::kMaxUsername) == name;
}

}  // namespace guest
