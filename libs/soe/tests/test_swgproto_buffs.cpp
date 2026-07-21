// Tests for AddBuffMessage (0x229) and RemoveBuffMessage (0x22A), Phase 4
// step 11's ObjControllerMessage sub-types - the generic Buff system
// (BuffImplementation.cpp is the sole real caller for both). AddBuffMessage
// confirmed live 2026-07-18 via a real /burstrun command, zero leftover
// bytes. RemoveBuffMessage is SYNTHETIC ONLY - burstrun's real cooldown
// made timing a live capture unreliable within this session's capture
// windows (a much lower-risk gap than CommandQueueAdd/Flourish, since its
// only field is the same buffcrc type already confirmed live via
// AddBuffMessage's sibling field, not a whole undecoded protocol).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/Buffs.h"

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

TEST_CASE("AddBuffMessage::parse - real payload, /burstrun broadcast") {
    // Captured 2026-07-18 via dummyclient --send-command burstrun
    // --command-args "". Bytes below start after the shared
    // ObjControllerMessage envelope (header1/header2/objectId/unused
    // already consumed elsewhere).
    auto buf = bufferFromHex("b2 1c 3d fc 00 00 f0 41");

    auto msg = AddBuffMessage::parse(buf);
    CHECK(msg.buffCrc == 0xfc3d1cb2);
    CHECK(msg.duration == doctest::Approx(30.0f));
    CHECK(buf.remaining() == 0);
}

TEST_CASE("RemoveBuffMessage::parse - synthetic payload pins field order") {
    // Reuses the real buffcrc value from the AddBuffMessage fixture above -
    // the same buff being removed, in spirit.
    auto buf = bufferFromHex("b2 1c 3d fc");

    auto msg = RemoveBuffMessage::parse(buf);
    CHECK(msg.buffCrc == 0xfc3d1cb2);
    CHECK(buf.remaining() == 0);
}
