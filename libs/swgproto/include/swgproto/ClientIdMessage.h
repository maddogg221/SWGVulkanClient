#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swgproto {

// Builds the opCount(2)+hash(4)+fields payload for ClientIdMsg, ready to
// hand to SoeSession::sendMessage(). This is the missing step between the
// zone handshake and SelectCharacter: it presents the account's identity
// (accountId + the sessionToken from LoginClientToken) to the ZONE server
// so it can associate this new UDP connection with the account and its
// character list - without it, SelectCharacter fails with "You are unable
// to login with this character" because the zone server has no idea which
// account the connection belongs to (see DISCOVERY.txt).
//
// NOTE: the message name hashed here is "ClientIdMsg", not "ClientIdMessage"
// (the C++ class name) - Core3's own source hashes the shorter string.
std::vector<uint8_t> buildClientIdMessage(uint32_t accountId,
                                           const std::vector<uint8_t>& sessionToken,
                                           const std::string& version = "20050408-18:00");

} // namespace swgproto
