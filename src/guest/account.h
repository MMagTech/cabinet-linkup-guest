// Who this friend is to a LinkUp server (PROTOCOL.md, "Who is joining").
//
// A console's account id is a hash of its RomM account, so a person is the same on every console
// they sign in on. 2P Start has no RomM. Its id is a hash of the first code it pairs with, kept
// beside the key: the invite a friend first uses becomes their own code, which the server takes only
// from that same id, so the same code on another computer, or after a reinstall, is the same friend
// (PROTOCOL.md, "One account on more than one console"). Once kept, the id stays, so a friend paired
// again with a changed code is still themself. The username is what the friend typed, and is what
// the host sees.

#pragma once

#include <string>

namespace guest {

// The id kept at pairing: 32 hex characters, or empty for none yet.
std::string accountId(const std::string& folder);

// The id to pair with: the kept one, or with none, the one this code gives. Empty when the code
// is not one.
std::string accountFor(const std::string& folder, const std::string& code);
// Kept once pairing has worked, unless one is kept already.
bool keepAccount(const std::string& folder, const std::string& id);

// The name given at pairing. Empty for none yet.
std::string username(const std::string& folder);
bool keepUsername(const std::string& folder, const std::string& name);

// Whether a server takes this as a username: 1 to 128 bytes of sound UTF-8, with no control or
// direction-changing characters.
bool usernameFits(const std::string& name);

}  // namespace guest
