// Who this friend is to a LinkUp server (PROTOCOL.md, "Who is joining").
//
// A console's account id is a hash of its RomM account. 2P Start has no RomM, so it makes one
// random id the first time it is asked and keeps it beside the key: the server knows the same
// friend by it every time. The username is what the friend typed, and is what the host sees.

#pragma once

#include <string>

namespace guest {

// 32 hex characters, made and kept on first use. Empty when the folder cannot hold it.
std::string accountId(const std::string& folder);

// The name given at pairing. Empty for none yet.
std::string username(const std::string& folder);
bool keepUsername(const std::string& folder, const std::string& name);

// Whether a server takes this as a username: 1 to 128 bytes of sound UTF-8, with no control or
// direction-changing characters.
bool usernameFits(const std::string& name);

}  // namespace guest
