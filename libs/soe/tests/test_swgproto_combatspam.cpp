// Test for CombatSpam (Phase 4 step 13's ObjControllerMessage sub-type,
// header2=0x134). CONFIRMED DEAD CODE on both known servers (Phase 4 Step
// 3/5 - zero real traffic across ~30,000+ captured packets spanning both
// taking and dealing lethal damage; this server's real combat feedback
// runs through ShowFlyText + CreatureObject deltas instead). SYNTHETIC
// ONLY - no real fixture is possible. Unlike CombatAction (deferred - see
// KNOWN_UNKNOWNS.md), this message's shape has zero ambiguity: both of
// CombatSpam.h's constructor overloads converge to the exact same wire
// layout.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CombatSpam.h"

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

TEST_CASE("CombatSpam::parse - synthetic payload pins field order") {
    auto buf = bufferFromHex(
        "28 de 06 01 00 00 01 00 "
        "29 de 06 01 00 00 01 00 "
        "00 00 00 00 00 00 00 00 "
        "96 00 00 00 "
        "08 00 63 62 74 5f 73 70 61 6d "
        "00 00 00 00 "
        "0a 00 61 74 74 61 63 6b 5f 68 69 74 "
        "0a "
        "00 00 00 00");

    auto msg = CombatSpam::parse(buf);
    CHECK(msg.attackerId == 281474993937960ULL);
    CHECK(msg.defenderId == 281474993937961ULL);
    CHECK(msg.itemId == 0);
    CHECK(msg.damage == 150);
    CHECK(msg.file == "cbt_spam");
    CHECK(msg.padding == 0);
    CHECK(msg.stringName == "attack_hit");
    CHECK(msg.color == 10);
    CHECK(msg.message.empty());
    CHECK(buf.remaining() == 0);
}
