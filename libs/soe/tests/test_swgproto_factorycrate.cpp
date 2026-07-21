// Permanent regression test for FactoryCrate BASE3/6 (Phase 6 Tier 3 remainder).
// Real fixtures captured live against Naritus 2026-07-17 via
// `/object createitem object/factory/factory_crate_generic_items.iff 5` - an
// empty (no contained prototype) 5-unit crate, admin-spawned directly (unlike
// ResourceContainer, FactoryCrate DOES compose from a generic
// SharedTangibleObjectTemplate, so plain `createitem` works here). Both
// fixtures independently decoded via a from-scratch Node.js script before
// being pinned, zero leftover bytes either way.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/FactoryCrateBaseline3.h"
#include "swgproto/FactoryCrateBaseline6.h"
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

TEST_CASE("FactoryCrateBaseline3::parse - real Naritus fixture (5x generic items crate)") {
    auto buf = bufferFromHex(
        "00 00 80 3f 09 00 66 61 63 74 6f 72 79 5f 6e 00 00 00 00 13 00 67 65 6e 65 72 69 63 5f "
        "69 74 65 6d 73 5f 63 72 61 74 65 26 00 00 00 47 00 65 00 6e 00 65 00 72 00 69 00 63 00 "
        "20 00 49 00 74 00 65 00 6d 00 73 00 20 00 43 00 72 00 61 00 74 00 65 00 20 00 28 00 53 "
        "00 79 00 73 00 74 00 65 00 6d 00 20 00 47 00 65 00 6e 00 65 00 72 00 61 00 74 00 65 00 "
        "64 00 29 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 21 00 00 05 00 00 00 00 00 00 "
        "00 e8 03 00 00 01");

    auto result = FactoryCrateBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.constantOne == doctest::Approx(1.0f));
    CHECK(b.objectName.file == "factory_n");
    CHECK(b.objectName.stringId == "generic_items_crate");
    CHECK(b.customObjectName == u"Generic Items Crate (System Generated)");
    CHECK(b.volume == 1);
    CHECK(b.customizationString.empty());
    CHECK(b.unknownField0 == 0);
    CHECK(b.unknownField1 == 0);
    CHECK(b.optionsBitmask == 8448);
    CHECK(b.useCount == 5);
    CHECK(b.conditionDamage == 0);
    CHECK(b.maxCondition == 1000);
    CHECK(b.objectVisible == true);

    CHECK(buf.remaining() == 0);
}

TEST_CASE("FactoryCrateBaseline6::parse - real Naritus fixture (all-constant stub)") {
    auto buf = bufferFromHex(
        "03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00");

    auto result = FactoryCrateBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.constantThree == 3);
    CHECK(b.unknownField0 == 0);
    CHECK(b.unknownField1 == 0);
    CHECK(b.unknownField2 == 0);
    CHECK(b.unknownField3 == 0);
    CHECK(b.unknownField4 == 0);
    CHECK(b.unknownField5 == 0);
    CHECK(b.unknownField6 == 0);
    CHECK(b.unknownField7 == 0);
    CHECK(b.unknownByte == 0);

    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - FactoryCrate BASE3 useCount (0x07)") {
    // Synthetic (not a real capture - no live quantity-change delta was
    // observed this session, though setQuantity() is confirmed live/
    // multiply-reachable from source - see FactoryCrateBaseline3.h). Field
    // index 0x07 is a literal from FactoryCrateObjectDeltaMessage3.h's
    // setQuantity(), NOT this field's struct position (0x08) - same
    // position-independent-index rule as every other schema here.
    std::vector<uint8_t> bytes = {0x07, 0x00, 0x03, 0x00, 0x00, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kFactoryCrateBaseline3Schema, 1, buf);

    CHECK_FALSE(decoded.stoppedEarly);
    REQUIRE(decoded.updates.size() == 1);
    CHECK(decoded.updates[0].fieldIndex == 0x07);
    CHECK(decoded.updates[0].fieldName == "useCount");
    CHECK(decoded.updates[0].valueText == "3");
}

TEST_CASE("decodeDeltaMessage - FactoryCrate BASE3 stops at unmapped index") {
    // Every field except useCount (0x07) has no delta path at all - pins
    // that a delta claiming an unmapped index is correctly rejected.
    std::vector<uint8_t> bytes = {0x00, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kFactoryCrateBaseline3Schema, 1, buf);

    CHECK(decoded.stoppedEarly);
    CHECK(decoded.updates.empty());
}
