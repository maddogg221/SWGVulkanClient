#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swgproto {

// Builds the opCount(2)+hash(4)+fields payload for ClientRandomNameRequest,
// ready to hand to SoeSession::sendMessage(). Wire layout: ASCII
// raceTemplate (e.g. "object/creature/player/human_male.iff").
std::vector<uint8_t> buildClientRandomNameRequestPacket(const std::string& raceTemplate);

} // namespace swgproto
