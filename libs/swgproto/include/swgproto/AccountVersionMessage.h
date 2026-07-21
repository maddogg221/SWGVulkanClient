#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swgproto {

// Builds the opCount(2)+hash(4)+fields payload for AccountVersionMessage,
// ready to hand to SoeSession::sendMessage(). Per DISCOVERY.txt, a real
// client does NOT send the trailing hash("SWGEmu") field that Core3's own
// source comments call "required" - a real capture against Finalizer showed
// the message ending right after the version string, and Core3's own
// server-side parser never reads a 4th field anyway.
std::vector<uint8_t> buildAccountVersionMessage(const std::string& username,
                                                 const std::string& password,
                                                 const std::string& version = "20050408-18:00");

} // namespace swgproto
