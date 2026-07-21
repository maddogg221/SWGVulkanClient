#pragma once

#include <cstdint>
#include <vector>

#include "soe/MessageHash.h"

namespace swgproto {

// hashCode("CmdSceneReady") - already verified in libs/soe's MessageHash
// tests (0x43FD1C22). Used both to build the client's outgoing message and
// to recognize the server's own CmdSceneReady reply, which flips the
// "scene ready" flag client-side (two distinct messages share this opcode:
// one client->server, one server->client - see DISCOVERY.txt).
constexpr uint32_t kCmdSceneReadyHash = soe::MessageHash::compute("CmdSceneReady");

// Builds the opCount(2)+hash(4) payload for CmdSceneReady - no fields, an
// empty body beyond the header.
std::vector<uint8_t> buildCmdSceneReady();

} // namespace swgproto
