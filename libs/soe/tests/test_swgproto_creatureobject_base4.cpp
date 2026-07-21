// Permanent regression test for CreatureObject BASE4 (Phase 6 continuation
// after BASE6/WeaponObject) - movement/encumbrance/skill-mod data. No
// TangibleObject ancestor (TangibleObject has no BASE4 file in source), so
// this is a standalone schema like BASE1.
//
// Unlike every other CreatureObject baseline decoded so far, BASE4's
// BASELINE genuinely IS self-only by server design - confirmed directly
// from CreatureObjectImplementation::sendBaselinesTo(), which only
// constructs a CreatureObjectMessage4 `if (player == thisPointer)`. A live
// probe (two full captures) never saw BASE4 for any objectId other than
// self's own, consistent with that source read. BASE4's DELTA is NOT fully
// self-only, though: sendSpeedAndAccelerationMods()/
// broadcastSpeedAndAccelerationMods() unicast a narrower
// CreatureObjectDeltaMessage4 (accelerationMultiplierMod/
// speedMultiplierMod/turnScale only) to every other real PLAYER creature
// nearby - so the delta dispatch handler is deliberately NOT self-gated.
//
// The real fixture below (316 bytes, self's own character, a
// crafting_artisan) was captured via the established temporary hex-dump
// method and independently cross-checked with a from-scratch Node.js
// decode script (not just re-reading this project's own parser output) -
// every field below, including all 8 skillMods entries and the exact float
// bit patterns for runSpeed/slopeModAngle, was verified against that
// independent decode before being pinned, specifically to avoid repeating
// this project's past mistakes (a dropped/extra byte during hand-wrapping,
// and a hand-derived expected value that was simply wrong) - see
// SESSION_LOG.md's Phase 6 entries for both prior incidents.
#include <doctest/doctest.h>

#include <cstring>
#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CreatureObjectBaseline4.h"
#include "swgproto/CreatureObjectDelta.h"

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

TEST_CASE("CreatureObjectBaseline4::parse - real self payload (crafting_artisan)") {
    auto buf = bufferFromHex(
        "00 00 80 3f 00 00 80 3f 03 00 00 00 04 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00 08 00 00 00 "
        "0a 00 00 00 00 13 00 61 72 6d 6f 72 5f 63 75 73 "
        "74 6f 6d 69 7a 61 74 69 6f 6e 14 00 00 00 00 00 "
        "00 00 00 16 00 63 6c 6f 74 68 69 6e 67 5f 63 75 "
        "73 74 6f 6d 69 7a 61 74 69 6f 6e 14 00 00 00 00 "
        "00 00 00 00 10 00 67 65 6e 65 72 61 6c 5f 61 73 "
        "73 65 6d 62 6c 79 14 00 00 00 00 00 00 00 00 17 "
        "00 67 65 6e 65 72 61 6c 5f 65 78 70 65 72 69 6d "
        "65 6e 74 61 74 69 6f 6e 23 00 00 00 00 00 00 00 "
        "00 19 00 6c 61 6e 67 75 61 67 65 5f 62 61 73 69 "
        "63 5f 63 6f 6d 70 72 65 68 65 6e 64 64 00 00 00 "
        "00 00 00 00 00 14 00 6c 61 6e 67 75 61 67 65 5f "
        "62 61 73 69 63 5f 73 70 65 61 6b 64 00 00 00 00 "
        "00 00 00 00 0a 00 6c 65 61 64 65 72 73 68 69 70 "
        "0a 00 00 00 00 00 00 00 00 09 00 73 75 72 76 65 "
        "79 69 6e 67 14 00 00 00 00 00 00 00 00 00 80 3f "
        "00 00 80 3f 00 00 00 00 00 00 00 00 31 08 ac 40 "
        "96 56 e8 3e 00 00 00 00 00 00 80 3f a2 45 c6 3f "
        "00 00 40 3f 00 00 00 00 00 00 00 00 ");

    auto result = CreatureObjectBaseline4::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.accelerationMultiplierBase == doctest::Approx(1.0f));
    CHECK(b.accelerationMultiplierMod == doctest::Approx(1.0f));

    REQUIRE(b.encumbrances.size() == 3);
    CHECK(b.encumbrances[0] == 0);
    CHECK(b.encumbrances[1] == 0);
    CHECK(b.encumbrances[2] == 0);

    REQUIRE(b.skillMods.size() == 8);
    CHECK(b.skillMods[0].key == "armor_customization");
    CHECK(b.skillMods[0].skillMod == 20);
    CHECK(b.skillMods[0].skillBonus == 0);
    CHECK(b.skillMods[3].key == "general_experimentation");
    CHECK(b.skillMods[3].skillMod == 35);
    CHECK(b.skillMods[4].key == "language_basic_comprehend");
    CHECK(b.skillMods[4].skillMod == 100);
    CHECK(b.skillMods[6].key == "leadership");
    CHECK(b.skillMods[6].skillMod == 10);
    CHECK(b.skillMods[7].key == "surveying");
    CHECK(b.skillMods[7].skillMod == 20);

    CHECK(b.speedMultiplierBase == doctest::Approx(1.0f));
    CHECK(b.speedMultiplierMod == doctest::Approx(1.0f));
    CHECK(b.listenId == 0);
    CHECK(b.runSpeed == doctest::Approx(5.376f));
    CHECK(b.slopeModAngle == doctest::Approx(0.4537856f));
    CHECK(b.slopeModPercent == doctest::Approx(0.0f));
    CHECK(b.turnScale == doctest::Approx(1.0f));
    CHECK(b.walkSpeed == doctest::Approx(1.549f));
    CHECK(b.waterModPercent == doctest::Approx(0.75f));
    CHECK(b.spaceMissionObjects.empty());

    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeCreatureObjectDelta - CreatureObject BASE4 confirmed scalar fields") {
    // Synthetic (not a real capture): index 0x01 (accelerationMultiplierMod)
    // + index 0x0A (turnScale), the two fields
    // CreatureObjectImplementation::sendSpeedAndAccelerationMods() actually
    // bundles together on the wire (see this file's header comment) - a
    // regression guard on delta field-index wiring, not the baseline wire
    // shape (already covered by the real fixture above).
    std::vector<uint8_t> bytes;
    auto pushU16 = [&](uint16_t v) {
        bytes.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto pushFloat = [&](float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        bytes.push_back(static_cast<uint8_t>(bits & 0xFF));
        bytes.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
    };

    pushU16(0x01);
    pushFloat(1.25f);
    pushU16(0x0A);
    pushFloat(1.0f);

    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeCreatureObjectDelta(4, 2, buf);

    CHECK_FALSE(decoded.stoppedEarly);
    REQUIRE(decoded.updates.size() == 2);
    CHECK(decoded.updates[0].fieldIndex == 0x01);
    CHECK(decoded.updates[0].fieldName == "accelerationMultiplierMod");
    CHECK(decoded.updates[1].fieldIndex == 0x0A);
    CHECK(decoded.updates[1].fieldName == "turnScale");
}

TEST_CASE("decodeCreatureObjectDelta - CreatureObject BASE4 stops at unmapped skillMods index") {
    // encumbrances (0x02)/skillMods (0x03)/spaceMissionObjects (0x0D) are
    // deliberately fieldBaselineOnly (see CreatureObjectBaseline4.h) - this
    // pins that a delta claiming index 0x03 is correctly rejected rather
    // than silently misdecoded, matching this project's "detect and stop,
    // don't guess" doctrine.
    std::vector<uint8_t> bytes = {0x03, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeCreatureObjectDelta(4, 1, buf);

    CHECK(decoded.stoppedEarly);
    CHECK(decoded.updates.empty());
}
