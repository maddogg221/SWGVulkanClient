// Test for Emote (Phase 4 step 10's ObjControllerMessage sub-type,
// header2=0x12E). A social emote (e.g. "/wave"), triggered client-side via
// the "socialInternal" QueueCommand
// (ChatManagerImplementation::handleSocialInternalMessage). Confirmed live
// 2026-07-18 by sending a real socialInternal command and precisely
// re-decoding the resulting broadcast with a throwaway Node.js script
// (zero leftover bytes, matching Emote.h's constructor exactly).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/Emote.h"

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

TEST_CASE("Emote::parse - real payload, /socialInternal broadcast") {
    // Captured 2026-07-18 via dummyclient --send-command socialInternal
    // --command-args "281474993937960 1 1 1". Bytes below start after the
    // shared ObjControllerMessage envelope (header1/header2/objectId/unused
    // already consumed elsewhere).
    auto buf = bufferFromHex(
        "28 de 06 01 00 00 01 00 28 de 06 01 00 00 01 00 01 00 00 00 03");

    auto msg = Emote::parse(buf);
    CHECK(msg.senderId == 281474993937960ULL);
    CHECK(msg.emoteTargetId == 281474993937960ULL);
    CHECK(msg.emoteId == 1);
    CHECK(msg.animTextFlags == 3);
    CHECK(buf.remaining() == 0);
}
