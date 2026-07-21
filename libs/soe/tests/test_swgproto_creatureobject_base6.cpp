// Permanent regression tests for CreatureObject BASE6 (self AND non-self -
// Phase 6's top-priority item, the first CreatureObject baseline decoded
// beyond BASE1/BASE3) and its prerequisite ancestor, TangibleObjectBaseline6.
// The real fixture below (573... no, 345 bytes, character "Kalda Ulzo",
// Finalizer) is a live self CREO BASE6 payload captured via the established
// temporary hex-dump method, independently re-verified field-by-field via
// PowerShell's [BitConverter] before being pinned - it is also the
// empirical record that corrected a wrong initial assumption about the
// `wearables` field's wire shape: a real captured payload with 9 non-empty
// entries revealed WearablesDeltaVector overrides the default
// "ManagedReference -> uint64 ID" shape with a richer 4-field per-entry
// format (see WearableEntry.h) - the ONLY reason this was caught before
// shipping is that this fixture's character happened to have real equipped
// items, unlike the ~249 other real objects seen in the same live run whose
// empty wearables lists never exercised the per-item format at all.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CreatureObjectBaseline6.h"
#include "swgproto/CreatureObjectDelta.h"
#include "swgproto/SchemaEngine.h"
#include "swgproto/TangibleObjectBaseline6.h"

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

TEST_CASE("TangibleObjectBaseline6::parse - real ancestor payload (empty defenders)") {
    // The ancestor portion of the fixture below, isolated: unknownConst
    // (literal 0x76 per source) + an empty defenders list (count=0).
    auto buf = bufferFromHex("76 00 00 00 00 00 00 00 b4 02 00 00");

    auto result = TangibleObjectBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& t = result.value();

    CHECK(t.unknownConst == 0x76);
    CHECK(t.defenders.empty());
    CHECK(buf.remaining() == 0);
}

TEST_CASE("CreatureObjectBaseline6::parse - real BASE6 payload (Kalda Ulzo ghost, Finalizer)") {
    // Captured 2026-07-16 via a temporary hex-dump patch during live
    // verification. 345 bytes, independently re-verified field-by-field
    // via PowerShell's [BitConverter] before being pinned - zero leftover
    // bytes, every field matching the live console output exactly.
    auto buf = bufferFromHex(
        "76 00 00 00 00 00 00 00 b4 02 00 00 05 00 00 "
        "00 07 00 6e 65 75 74 72 61 6c 65 85 7f d3 00 "
        "00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 09 00 00 00 94 58 00 00 a2 02 00 00 "
        "76 01 00 00 5d 01 00 00 8a 02 00 00 77 01 00 "
        "00 77 01 00 00 e8 03 00 00 f4 01 00 00 84 03 "
        "00 00 09 00 00 00 77 00 00 00 07 03 00 00 77 "
        "01 00 00 5e 01 00 00 ee 02 00 00 77 01 00 00 "
        "77 01 00 00 e8 03 00 00 f4 01 00 00 84 03 00 "
        "00 09 00 00 00 0c 00 00 00 00 00 04 00 00 00 "
        "58 87 cf d3 00 00 01 00 3b e8 69 39 00 00 04 "
        "00 00 00 59 87 cf d3 00 00 01 00 01 50 ba 73 "
        "00 00 04 00 00 00 5b 87 cf d3 00 00 01 00 11 "
        "97 b7 70 06 00 01 01 01 04 ff 03 04 00 00 00 "
        "5d 87 cf d3 00 00 01 00 aa fd d2 1a 00 00 04 "
        "00 00 00 5e 87 cf d3 00 00 01 00 35 cc 57 c3 "
        "00 00 04 00 00 00 5f 87 cf d3 00 00 01 00 f3 "
        "c0 1a 77 00 00 04 00 00 00 60 87 cf d3 00 00 "
        "01 00 1c 78 af 59 00 00 04 00 00 00 61 87 cf "
        "d3 00 00 01 00 9e 02 12 9c 00 00 04 00 00 00 "
        "65 85 7f d3 00 00 01 00 ce 88 26 65 00 00 00 ");

    auto result = CreatureObjectBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.tangible.unknownConst == 0x76);
    CHECK(b.tangible.defenders.empty());
    CHECK(b.level == 5);
    CHECK(b.performanceAnimation.empty());
    CHECK(b.moodString == "neutral");
    CHECK(b.weaponId == 281478525060453ULL);
    CHECK(b.groupId == 0);
    CHECK(b.groupInviterAndCounter == std::array<uint64_t, 2>{0, 0});
    CHECK(b.guildId == 0);
    CHECK(b.targetId == 0);
    CHECK(b.moodId == 0);
    CHECK(b.performanceStartTime == 0);
    CHECK(b.performanceType == 0);
    CHECK(b.ham == std::vector<int32_t>{674, 374, 349, 650, 375, 375, 1000, 500, 900});
    CHECK(b.maxHam == std::vector<int32_t>{775, 375, 350, 750, 375, 375, 1000, 500, 900});
    CHECK(b.alternateAppearance.empty());
    CHECK(b.frozen == 0);

    // wearables: 9 real equipped items - the field this fixture exists to
    // pin down. Spot-check a plain entry and the one entry with a real
    // non-empty (non-printable) customizationString, rather than
    // enumerating all 9.
    REQUIRE(b.wearables.size() == 9);
    CHECK(b.wearables[0].customizationString.empty());
    CHECK(b.wearables[0].containmentType == 4);
    CHECK(b.wearables[0].objectId == 281478530303832ULL);
    CHECK(b.wearables[0].clientObjectCrc == 963242043);
    CHECK(b.wearables[3].customizationString.size() == 6); // non-printable dye-like bytes
    CHECK(b.wearables[3].objectId == 281478530303837ULL);
    CHECK(b.wearables[8].objectId == 281478525060453ULL); // matches weaponId - the equipped weapon

    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeCreatureObjectDelta - BASE6 confirmed-addressable scalar fields") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0B); // performanceStartTime
    buf.writeUint32(1234);
    buf.writeUint16(0x0C); // performanceType
    buf.writeUint32(2);

    auto result = decodeCreatureObjectDelta(6, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "performanceStartTime");
    CHECK(result.updates[0].valueText == "1234");
    CHECK(result.updates[1].fieldName == "performanceType");
    CHECK(result.updates[1].valueText == "2");
}

TEST_CASE("decodeCreatureObjectDelta - BASE6 stops at fields with no confirmed delta shape") {
    // Regression guard for real live traffic seen during verification: a
    // real delta arrived at field index 0x0D (ham) for a non-self creature,
    // despite CreatureObjectDeltaMessage6.h giving no setter for it (HAM is
    // a self-serializing DeltaVector<int>, not routed through this delta
    // message class) - the engine correctly detected and stopped rather
    // than guessing at its shape.
    soe::PacketBuffer buf;
    buf.writeUint16(0x08); // guildId - known
    buf.writeUint32(5);
    buf.writeUint16(0x0D); // ham - baseline-only, no confirmed delta shape
    buf.writeUint32(0);

    auto result = decodeCreatureObjectDelta(6, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "guildId");
}
