// Permanent regression tests for the Phase 2 step 4 CreatureObject BASE1/
// BASE3 parsers. Unlike this project's usual swgproto convention (verify
// live against the real server, no unit tests - see BaselineEnvelope's
// history in DISCOVERY.txt), these two fixtures use byte-for-byte REAL
// payloads captured from a live Finalizer session (character "Kalda
// Ulzo") via a temporary hex-dump patch, not synthetic/hand-built buffers -
// added specifically because this is the first step parsing real game
// data (credits, name, posture, HAM, wounds, etc.), and a prior version of
// CreatureObjectBaseline1::parse had a real bug (a missing 8-byte SkillList
// header) that only a live run caught. These fixtures pin that fix in
// place. The delta-decode cases are synthetic (no real self-delta happened
// to arrive during this project's live sessions), since the wire format
// there is simple index+value pairs built from primitives the baseline
// fixtures already exercise against real bytes.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CreatureObjectBaseline1.h"
#include "swgproto/CreatureObjectBaseline3.h"
#include "swgproto/CreatureObjectDelta.h"

using namespace swgproto;

namespace {

// Turns a "xx xx xx ..." hex string into a PacketBuffer positioned at the
// start - lets the real-capture fixtures below stay readable instead of a
// giant uint8_t initializer list.
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

TEST_CASE("CreatureObjectBaseline1::parse - real BASE1 payload (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15 via a temporary hex-dump patch on a real zone-in.
    auto buf = bufferFromHex(
        "23 20 00 00 00 00 00 00 09 00 00 00 1f 00 00 00 07 03 00 00 77 01 00 00 5e 01 00 00 "
        "ee 02 00 00 77 01 00 00 77 01 00 00 e8 03 00 00 f4 01 00 00 84 03 00 00 09 00 00 00 "
        "04 00 00 00 15 00 63 6f 6d 62 61 74 5f 62 72 61 77 6c 65 72 5f 6e 6f 76 69 63 65 0e "
        "00 73 70 65 63 69 65 73 5f 7a 61 62 72 61 6b 20 00 73 6f 63 69 61 6c 5f 6c 61 6e 67 "
        "75 61 67 65 5f 62 61 73 69 63 5f 63 6f 6d 70 72 65 68 65 6e 64 1b 00 73 6f 63 69 61 "
        "6c 5f 6c 61 6e 67 75 61 67 65 5f 62 61 73 69 63 5f 73 70 65 61 6b 21 00 73 6f 63 69 "
        "61 6c 5f 6c 61 6e 67 75 61 67 65 5f 7a 61 62 72 61 6b 5f 63 6f 6d 70 72 65 68 65 6e "
        "64 1c 00 73 6f 63 69 61 6c 5f 6c 61 6e 67 75 61 67 65 5f 7a 61 62 72 61 6b 5f 73 70 "
        "65 61 6b 19 00 63 6f 6d 62 61 74 5f 62 72 61 77 6c 65 72 5f 75 6e 61 72 6d 65 64 5f "
        "30 31 19 00 63 6f 6d 62 61 74 5f 62 72 61 77 6c 65 72 5f 75 6e 61 72 6d 65 64 5f 30 "
        "32 19 00 63 6f 6d 62 61 74 5f 62 72 61 77 6c 65 72 5f 75 6e 61 72 6d 65 64 5f 30 33");

    auto result = CreatureObjectBaseline1::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.bankCredits == 8227);
    CHECK(b.cashCredits == 0);
    CHECK(b.baseHam == std::vector<int32_t>{775, 375, 350, 750, 375, 375, 1000, 500, 900});

    std::vector<std::string> expectedSkills = {
        "combat_brawler_novice",
        "species_zabrak",
        "social_language_basic_comprehend",
        "social_language_basic_speak",
        "social_language_zabrak_comprehend",
        "social_language_zabrak_speak",
        "combat_brawler_unarmed_01",
        "combat_brawler_unarmed_02",
        "combat_brawler_unarmed_03",
    };
    CHECK(b.skillList == expectedSkills);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("CreatureObjectBaseline3::parse - real BASE3 payload (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15 via a temporary hex-dump patch on the same
    // zone-in as the BASE1 fixture above. Byte offsets independently
    // re-verified field-by-field (complexity, objectName StringId, name,
    // volume, customizationString length, empty visibleComponents,
    // condition/posture/etc., 9-item wounds) before being pinned here.
    auto buf = bufferFromHex(
        "00 00 c8 42 07 00 73 70 65 63 69 65 73 00 00 00 00 06 00 7a 61 62 72 61 6b 0a 00 00 "
        "00 4b 00 61 00 6c 00 64 00 61 00 20 00 55 00 6c 00 7a 00 6f 00 01 00 00 00 5a 00 01 "
        "23 17 94 18 ff 01 1c 0a 1b ff 01 05 ff 01 1a 70 19 ff 01 0d 14 09 ff 01 12 ff 01 13 "
        "5c 20 19 10 33 21 ff 01 0f ff 01 14 22 11 42 0e 8f 03 3d 0b ff 01 0c ff 01 06 ff 01 "
        "08 c2 15 70 16 ff 01 04 4c 07 ff 01 0a 1e 36 03 25 ff 01 35 04 24 01 2a ff 01 01 0b "
        "22 ff 01 ff 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 e8 03 00 "
        "00 01 00 00 00 00 00 00 00 00 00 00 64 3b 7f 3f 00 00 00 00 00 00 00 00 00 00 00 00 "
        "09 00 00 00 b3 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

    auto result = CreatureObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();
    const auto& t = b.tangible;

    CHECK(t.complexity == doctest::Approx(100.0f));
    CHECK(t.objectName.file == "species");
    CHECK(t.objectName.stringId == "zabrak");
    CHECK(t.customObjectName == u"Kalda Ulzo");
    CHECK(t.volume == 1);
    CHECK(t.customizationString.size() == 90);
    CHECK(t.visibleComponents.empty());
    CHECK(t.optionsBitmask == 0);
    CHECK(t.useCount == 0);
    CHECK(t.conditionDamage == 0);
    CHECK(t.maxCondition == 1000);
    CHECK(t.objectVisible == true);

    CHECK(b.posture == 0);
    CHECK(b.factionRank == 0);
    CHECK(b.creatureLinkId == 0);
    CHECK(b.height == doctest::Approx(0.997f));
    CHECK(b.shockWounds == 0);
    CHECK(b.stateBitmask == 0);
    CHECK(b.wounds == std::vector<int32_t>{0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeCreatureObjectDelta - BASE1 scalar fields") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0000); // bankCredits
    buf.writeUint32(12345);
    buf.writeUint16(0x0001); // cashCredits
    buf.writeUint32(999);

    auto result = decodeCreatureObjectDelta(1, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "bankCredits");
    CHECK(result.updates[0].valueText == "12345");
    CHECK(result.updates[1].fieldName == "cashCredits");
    CHECK(result.updates[1].valueText == "999");
}

TEST_CASE("decodeCreatureObjectDelta - BASE3 mixed scalar + container skip") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0008); // conditionDamage
    buf.writeUint32(50);
    buf.writeUint16(0x0011); // wounds container
    buf.writeUint32(3);      // count
    buf.writeUint32(7);      // updateCounter
    buf.writeUint32(1);
    buf.writeUint32(2);
    buf.writeUint32(3);
    buf.writeUint16(0x000D); // creatureLinkId
    buf.writeUint64(281478530303830ULL);

    auto result = decodeCreatureObjectDelta(3, 3, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 3);
    CHECK(result.updates[0].fieldName == "conditionDamage");
    CHECK(result.updates[0].valueText == "50");
    CHECK(result.updates[1].fieldName == "wounds");
    CHECK(result.updates[1].valueText == "<3 items skipped>");
    CHECK(result.updates[2].fieldName == "creatureLinkId");
    CHECK(result.updates[2].valueText == "281478530303830");
}

TEST_CASE("decodeCreatureObjectDelta - stops at unmapped field index") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0000); // bankCredits
    buf.writeUint32(1);
    buf.writeUint16(0x00FF); // unmapped for BASE1
    buf.writeUint32(0xDEADBEEF);

    auto result = decodeCreatureObjectDelta(1, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "bankCredits");
}
