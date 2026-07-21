// Test for Flourish (Phase 4 step 10's ObjControllerMessage sub-type,
// header2=0x166). SYNTHETIC ONLY - fires only during an active Entertainer
// performance (dancing/playing music); a real capture needs an Entertainer
// skill grant plus a valid dance/music performance name. Checked live
// 2026-07-18: an empty /startdance on the admin test character returned
// zero available dances (no Entertainer skill on hand) - disproportionate
// setup for this one low-priority sub-type, so implemented directly from
// Flourish.h's constructor instead (insertInt(flourishid), an unambiguous
// single-field shape), same treatment as CommandQueueAdd/CombatAction/
// CombatSpam. Revisit with a real fixture if an Entertainer character is
// ever set up.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/Flourish.h"

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

TEST_CASE("Flourish::parse - synthetic payload pins field order") {
    // 05 00 00 00 = flourishId 5.
    auto buf = bufferFromHex("05 00 00 00");

    auto msg = Flourish::parse(buf);
    CHECK(msg.flourishId == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("Flourish::parse - synthetic payload, outro sentinel value -1") {
    // ff ff ff ff = flourishId -1 (the outro sentinel from
    // EntertainingSessionImplementation.cpp: "new Flourish(player, -1)").
    auto buf = bufferFromHex("ff ff ff ff");

    auto msg = Flourish::parse(buf);
    CHECK(msg.flourishId == -1);
    CHECK(buf.remaining() == 0);
}
