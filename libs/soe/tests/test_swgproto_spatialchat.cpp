// Tests for SpatialChat (Phase 4 step 8's ObjControllerMessage sub-type,
// header2=0xF4). Real spoken chat, triggered client-side via the
// "spatialchatinternal" QueueCommand
// (ChatManagerImplementation::handleSpatialChatInternalMessage), broadcast
// to everyone in range. Confirmed live 2026-07-18 by sending a real
// /spatialchatinternal command over dummyclient and precisely re-decoding
// the resulting broadcast bytes with a throwaway Node.js script (zero
// leftover bytes, matching SpatialChat.h's raw-UnicodeString-message
// constructor variant exactly).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/SpatialChat.h"

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

TEST_CASE("SpatialChat::parse - real payload, /spatialchatinternal broadcast") {
    // Captured 2026-07-18 via dummyclient --send-command spatialchatinternal
    // --command-args "0 0 0 0 0 Hello from dummyclient". Bytes below start
    // after the shared ObjControllerMessage envelope (header1/header2/
    // objectId/unused already consumed elsewhere).
    auto buf = bufferFromHex(
        "28 de 06 01 00 00 01 00 00 00 00 00 00 00 00 00 "
        "16 00 00 00 "
        "48 00 65 00 6c 00 6c 00 6f 00 20 00 66 00 72 00 6f 00 6d 00 20 00 "
        "64 00 75 00 6d 00 6d 00 79 00 63 00 6c 00 69 00 65 00 6e 00 74 00 "
        "32 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

    auto msg = SpatialChat::parse(buf);
    CHECK(msg.senderId == 281474993937960ULL);
    CHECK(msg.chatTargetId == 0ULL);
    CHECK(msg.message == u"Hello from dummyclient");
    CHECK(msg.volume == 50);
    CHECK(msg.spatialChatType == 1);
    CHECK(msg.moodType == 0);
    CHECK(msg.chatFlags == 0);
    CHECK(msg.languageId == 0);
    CHECK(msg.unknownTrailingLong == 0);
    CHECK(msg.unknownTrailingInt1 == 0);
    CHECK(msg.unknownTrailingInt2 == 0);
    CHECK(buf.remaining() == 0);
}
