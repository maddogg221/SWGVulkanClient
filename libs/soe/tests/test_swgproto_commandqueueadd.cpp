// Test for CommandQueueAdd (Phase 4 step 9's ObjControllerMessage
// sub-type, header2=0x167). SYNTHETIC ONLY - unlike every other sub-type
// decoded in this project, no real capture exists for this one. Traced
// every real caller of CreatureObject::sendCommand() (the only path that
// emits this message) in Core3's source: all are server-initiated actions
// (stimpack/medpack use, grenades, traps, heavy-weapon-mount firing)
// reached exclusively via right-click radial menu selection
// (handleObjectMenuSelect) - a protocol this project hasn't decoded yet
// (ObjectMenuResponse, Phase 4 Step 12). Normal client-issued commands
// never trigger it (they go through a separate enqueueCommand() path).
// Implemented directly from CommandQueueAdd.h's constructor
// (insertInt(actioncnt) + insertInt(actionCRC), an unambiguous 8-byte
// shape) - same "implement from source, flag as unverified" treatment as
// CombatAction/CombatSpam (Step 13). Revisit with a real fixture once the
// radial-menu protocol exists.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CommandQueueAdd.h"

using namespace swgproto;

namespace {

soe::PacketBuffer bufferFromHex(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(hex);
    std::string token;
    while (iss >> token) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
    }
    return soe::PacketBuffer(bytes.data(), bytes.size());
}

} // namespace

TEST_CASE("CommandQueueAdd::parse - synthetic payload pins field order") {
    // 44 00 00 00 = actionCount 0x44 (68), 78 56 34 12 = actionCrc 0x12345678.
    auto buf = bufferFromHex("44 00 00 00 78 56 34 12");

    auto msg = CommandQueueAdd::parse(buf);
    CHECK(msg.actionCount == 68);
    CHECK(msg.actionCrc == 0x12345678);
    CHECK(buf.remaining() == 0);
}
