// Permanent regression tests for standalone TangibleObject BASE3 (Phase 2
// step 9 - the fifth real object-type case, and the first NOT self-only:
// unlike CreatureObject/PlayerObject, TangibleObjectBaseline3 has been
// exercised only via CreatureObjectBaseline3's ancestor composition until
// now. The real fixture below is a live "Travel Terminal" object (FourCC
// TANO), captured via the established temporary hex-dump patch and
// independently re-verified field-by-field before being pinned - it is
// also the empirical confirmation that volume/visibleComponents/
// objectVisible's fieldBaselineOnly() correctness fix (see
// TangibleObjectBaseline3.h) still decodes the full real payload with zero
// leftover bytes.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/SchemaEngine.h"
#include "swgproto/TangibleObjectBaseline3.h"

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

TEST_CASE("TangibleObjectBaseline3::parse - real BASE3 payload (Travel Terminal, Finalizer)") {
    // Captured 2026-07-15 via a temporary hex-dump patch on a real zone-in.
    // 105 bytes, independently re-verified field-by-field before being
    // pinned - zero leftover bytes.
    auto buf = bufferFromHex(
        "00 00 c8 42 0d 00 74 65 72 6d 69 6e 61 6c 5f 6e 61 6d 65 00 00 00 00 0f 00 74 65 72 6d "
        "69 6e 61 6c 5f 74 72 61 76 65 6c 0f 00 00 00 54 00 72 00 61 00 76 00 65 00 6c 00 20 00 "
        "54 00 65 00 72 00 6d 00 69 00 6e 00 61 00 6c 00 01 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 01 00 00 00 00 00 00 00 00 00 00 e8 03 00 00 01");

    auto result = TangibleObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& t = result.value();

    CHECK(t.complexity == 100.0f);
    CHECK(t.objectName.file == "terminal_name");
    CHECK(t.objectName.stringId == "terminal_travel");
    CHECK(t.customObjectName == u"Travel Terminal");
    CHECK(t.volume == 1);
    CHECK(t.customizationString.empty());
    CHECK(t.visibleComponents.empty());
    CHECK(t.optionsBitmask == 256);
    CHECK(t.useCount == 0);
    CHECK(t.conditionDamage == 0);
    CHECK(t.maxCondition == 1000);
    CHECK(t.objectVisible == true);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - TangibleObject BASE3 confirmed-addressable scalar fields") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x08); // conditionDamage
    buf.writeUint32(50);
    buf.writeUint16(0x09); // maxCondition
    buf.writeUint32(1000);

    auto result = decodeDeltaMessage(kTangibleObjectBaseline3Schema, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "conditionDamage");
    CHECK(result.updates[0].valueText == "50");
    CHECK(result.updates[1].fieldName == "maxCondition");
    CHECK(result.updates[1].valueText == "1000");
}

TEST_CASE("decodeDeltaMessage - TangibleObject BASE3 stops at fields with no confirmed delta shape") {
    // Regression guard for this session's correctness fix: volume(0x03),
    // visibleComponents(0x05), and objectVisible(0x0A) have no setter in
    // TangibleObjectDeltaMessage3.h, so they're fieldBaselineOnly, not
    // field<> - a delta targeting any of them must stop, not silently
    // decode as if it were addressable.
    soe::PacketBuffer buf;
    buf.writeUint16(0x00); // complexity - known
    buf.writeUint32(200);
    buf.writeUint16(0x03); // volume - baseline-only, no confirmed delta shape
    buf.writeUint32(0);

    auto result = decodeDeltaMessage(kTangibleObjectBaseline3Schema, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "complexity");
}
