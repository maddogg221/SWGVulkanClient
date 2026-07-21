// Permanent regression tests for PlayerObject BASE8 (self-only), mirroring
// test_swgproto_playerobject.cpp's convention: a real byte-for-byte payload
// captured from a live Finalizer session (character "Kalda Ulzo", the self
// player's PlayerObject "ghost") via a temporary hex-dump patch, decoded
// field-by-field via PowerShell before being pinned as expected values.
// Split into its own file rather than extending test_swgproto_playerobject.cpp
// given BASE8's larger fixture (213 bytes, nested experienceList/waypointList
// entries).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/PlayerObjectBaseline8.h"
#include "swgproto/SchemaEngine.h"

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

TEST_CASE("PlayerObjectBaseline8::parse - real BASE8 payload (Kalda Ulzo ghost, Finalizer)") {
    // Captured 2026-07-15 via a temporary hex-dump patch. 213 bytes,
    // independently re-verified field-by-field (experienceList entries
    // matching the live console output exactly, waypointList's key ==
    // objectId in this real sample despite being written redundantly on
    // the wire, forcePower/quests/trailingUnknown all zero) before being
    // pinned here.
    auto buf = bufferFromHex(
        "02 00 00 00 3d 01 00 00 00 0e 00 63 6f 6d 62 61 74 5f 67 65 6e 65 72 61 6c b1 0a 00 "
        "00 00 1e 00 63 6f 6d 62 61 74 5f 6d 65 6c 65 65 73 70 65 63 69 61 6c 69 7a 65 5f 75 "
        "6e 61 72 6d 65 64 85 19 00 00 01 00 00 00 06 00 00 00 00 2a a4 e4 d3 00 00 01 00 00 "
        "00 00 00 00 60 92 c5 00 00 00 00 00 b0 76 45 00 00 00 00 00 00 00 00 58 b5 d7 af 18 "
        "00 00 00 40 00 75 00 69 00 3a 00 64 00 61 00 74 00 61 00 70 00 61 00 64 00 5f 00 6e "
        "00 65 00 77 00 5f 00 77 00 61 00 79 00 70 00 6f 00 69 00 6e 00 74 00 2a a4 e4 d3 00 "
        "00 01 00 01 01 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 01 00 00 "
        "00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00");

    auto result = PlayerObjectBaseline8::parse(buf);
    REQUIRE(result.ok());
    const auto& p = result.value();

    REQUIRE(p.experienceList.size() == 2);
    CHECK(p.experienceList[0].key == "combat_general");
    CHECK(p.experienceList[0].value == 2737);
    CHECK(p.experienceList[1].key == "combat_meleespecialize_unarmed");
    CHECK(p.experienceList[1].value == 6533);

    REQUIRE(p.waypointList.size() == 1);
    const auto& wp = p.waypointList[0];
    CHECK(wp.key == 281478531687466ULL);
    CHECK(wp.cellId == 0);
    CHECK(wp.x == doctest::Approx(-4684.0f));
    CHECK(wp.z == doctest::Approx(0.0f));
    CHECK(wp.y == doctest::Approx(3947.0f));
    CHECK(wp.unknown == 0);
    CHECK(wp.objectId == 281478531687466LL); // matches `key` in this real sample
    CHECK(wp.color == 1);
    CHECK(wp.active == 1);

    CHECK(p.forcePower == 0);
    CHECK(p.forcePowerMax == 0);
    CHECK(p.completedQuests.empty());
    CHECK(p.activeQuests.empty());
    CHECK(p.playerQuestsData.empty());
    CHECK(p.trailingUnknown == std::array<int32_t, 2>{0, 0});
    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - PLAY8 forcePower/forcePowerMax") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0002); // forcePower
    buf.writeUint32(150);
    buf.writeUint16(0x0003); // forcePowerMax
    buf.writeUint32(200);

    auto result = decodeDeltaMessage(kPlayerObjectBaseline8Schema, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "forcePower");
    CHECK(result.updates[0].valueText == "150");
    CHECK(result.updates[1].fieldName == "forcePowerMax");
    CHECK(result.updates[1].valueText == "200");
}

TEST_CASE("decodeDeltaMessage - PLAY8 stops at an unmapped field index") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0002); // forcePower - known
    buf.writeUint32(1);
    buf.writeUint16(0x0000); // experienceList - baseline-only, not delta-addressable
    buf.writeUint32(0);

    auto result = decodeDeltaMessage(kPlayerObjectBaseline8Schema, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "forcePower");
}
