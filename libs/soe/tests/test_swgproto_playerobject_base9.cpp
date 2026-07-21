// Permanent regression tests for PlayerObject BASE9 (self-only), mirroring
// test_swgproto_playerobject_base8.cpp's convention: a real byte-for-byte
// payload captured from a live Finalizer session (character "Kalda Ulzo")
// via a temporary hex-dump patch, decoded field-by-field via PowerShell
// before being pinned as expected values. This fixture is also the
// empirical confirmation that the unknownGap field's assumed 16-byte size
// (see PlayerObjectBaseline9.h's struct comment) is correct: the real
// payload consumes exactly 573 bytes with zero left over.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/PlayerObjectBaseline9.h"
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

TEST_CASE("PlayerObjectBaseline9::parse - real BASE9 payload (Kalda Ulzo ghost, Finalizer)") {
    // Captured 2026-07-15 via a temporary hex-dump patch. 573 bytes,
    // independently re-verified field-by-field via PowerShell before being
    // pinned here - 27 real ability names, all scalar fields, and the
    // unknownGap consuming exactly the assumed 16 bytes with zero leftover.
    auto buf = bufferFromHex(
        "1b 00 00 00 01 00 00 00 16 00 70 72 69 76 61 74 65 5f 62 72 61 77 6c 65 72 5f 6e 6f "
        "76 69 63 65 0d 00 70 6f 6c 65 61 72 6d 4c 75 6e 67 65 31 0d 00 75 6e 61 72 6d 65 64 "
        "4c 75 6e 67 65 31 0d 00 6d 65 6c 65 65 31 68 4c 75 6e 67 65 31 0d 00 6d 65 6c 65 65 "
        "32 68 4c 75 6e 67 65 31 05 00 74 61 75 6e 74 07 00 77 61 72 63 72 79 31 0b 00 69 6e "
        "74 69 6d 69 64 61 74 65 31 08 00 62 65 72 73 65 72 6b 31 11 00 63 65 72 74 5f 6b 6e "
        "69 66 65 5f 64 61 67 67 65 72 0d 00 63 65 6e 74 65 72 4f 66 42 65 69 6e 67 08 00 76 "
        "69 74 61 6c 69 7a 65 0b 00 65 71 75 69 6c 69 62 72 69 75 6d 10 00 63 65 72 74 5f 6b "
        "6e 69 66 65 5f 73 74 6f 6e 65 13 00 63 65 72 74 5f 6b 6e 69 66 65 5f 73 75 72 76 69 "
        "76 61 6c 18 00 63 65 72 74 5f 6c 61 6e 63 65 5f 73 74 61 66 66 5f 77 6f 6f 64 5f 73 "
        "31 13 00 63 65 72 74 5f 61 78 65 5f 68 65 61 76 79 5f 64 75 74 79 0f 00 63 65 72 74 "
        "5f 72 69 66 6c 65 5f 63 64 65 66 10 00 63 65 72 74 5f 70 69 73 74 6f 6c 5f 63 64 65 "
        "66 11 00 63 65 72 74 5f 63 61 72 62 69 6e 65 5f 63 64 65 66 20 00 63 65 72 74 5f 67 "
        "72 65 6e 61 64 65 5f 66 72 61 67 6d 65 6e 74 61 74 69 6f 6e 5f 6c 69 67 68 74 19 00 "
        "70 72 69 76 61 74 65 5f 62 72 61 77 6c 65 72 5f 75 6e 61 72 6d 65 64 5f 31 0b 00 75 "
        "6e 61 72 6d 65 64 48 69 74 31 19 00 70 72 69 76 61 74 65 5f 62 72 61 77 6c 65 72 5f "
        "75 6e 61 72 6d 65 64 5f 32 0c 00 75 6e 61 72 6d 65 64 53 74 75 6e 31 19 00 70 72 69 "
        "76 61 74 65 5f 62 72 61 77 6c 65 72 5f 75 6e 61 72 6d 65 64 5f 33 0d 00 75 6e 61 72 "
        "6d 65 64 42 6c 69 6e 64 31 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 c5 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 01 00 00 00 00 00 00 00 64 00 00 00 00 00 00 00 64 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00 00");

    auto result = PlayerObjectBaseline9::parse(buf);
    REQUIRE(result.ok());
    const auto& p = result.value();

    REQUIRE(p.abilityList.size() == 27);
    CHECK(p.abilityList[0] == "private_brawler_novice");
    CHECK(p.abilityList[1] == "polearmLunge1");
    CHECK(p.abilityList[26] == "unarmedBlind1");

    CHECK(p.experimentationEnabled == 0);
    CHECK(p.craftingState == 0);
    CHECK(p.nearestCraftingStation == 0);
    CHECK(p.schematics.empty());
    CHECK(p.experimentationPoints == 0);
    CHECK(p.speciesData == 0);
    CHECK(p.friendListStub == std::array<int32_t, 2>{0, 0});
    CHECK(p.ignoreListStub == std::array<int32_t, 2>{0, 0});
    CHECK(p.languageId == 1);
    CHECK(p.foodFilling == 0);
    CHECK(p.foodFillingMax == 100);
    CHECK(p.drinkFilling == 0);
    CHECK(p.drinkFillingMax == 100);
    CHECK(p.unknownGap == std::array<int32_t, 4>{0, 0, 0, 0});
    CHECK(p.jediState == 0);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - PLAY9 confirmed-addressable scalar fields") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0009); // languageId
    buf.writeUint32(2);
    buf.writeUint16(0x0011); // jediState
    buf.writeUint32(1);

    auto result = decodeDeltaMessage(kPlayerObjectBaseline9Schema, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "languageId");
    CHECK(result.updates[0].valueText == "2");
    CHECK(result.updates[1].fieldName == "jediState");
    CHECK(result.updates[1].valueText == "1");
}

TEST_CASE("decodeDeltaMessage - PLAY9 stops at fields with no confirmed delta shape") {
    // Real Finalizer traffic sends live deltas at indices 0x04 (schematics)
    // and 0x07 (friendListStub) despite source giving no confirmed live
    // evidence for either - see DISCOVERY.txt's noted follow-up. This
    // pins the current, deliberately conservative behavior: detect and
    // stop rather than guess at an unconfirmed shape.
    soe::PacketBuffer buf;
    buf.writeUint16(0x0009); // languageId - known
    buf.writeUint32(1);
    buf.writeUint16(0x0004); // schematics - real traffic sends this, shape unconfirmed
    buf.writeUint32(0);

    auto result = decodeDeltaMessage(kPlayerObjectBaseline9Schema, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "languageId");
}
