// Who this friend is to a LinkUp server (account.h).

#include "guest/account.h"

#include <array>
#include <cstdint>
#include <vector>

#include "box.h"
#include "fileio.h"
#include "hexutil.h"
#include "noise.h"
#include "pairing.h"

namespace guest {

namespace {

constexpr size_t kIdBytes = 16;

}  // namespace

std::string accountId(const std::string& folder) {
    const std::vector<uint8_t> held = cab::readBytes(folder + "/account");
    std::array<uint8_t, kIdBytes> raw{};
    const std::string kept(held.begin(), held.end());
    return hexutil::decode(kept, raw) ? kept : std::string();
}

std::string accountFor(const std::string& folder, const std::string& code) {
    const std::string kept = accountId(folder);
    if (!kept.empty()) return kept;
    // Tidied as pairing tidies it, so however it was typed, one code gives one id.
    const std::string twelve = pairing::tidy(code);
    if (twelve.empty()) return {};
    const noise::Key sum = noise::sha256("2P Start account 1|" + twelve);
    return hexutil::encode(sum.data(), kIdBytes);
}

bool keepAccount(const std::string& folder, const std::string& id) {
    std::array<uint8_t, kIdBytes> raw{};
    if (!accountId(folder).empty()) return true;
    return hexutil::decode(id, raw) && cab::writeBytes(folder + "/account", id, 0600);
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
