// Permanent regression test for CellObject BASE3/6 (Phase 6 Tier 2, "low
// hanging fruit" batch alongside GuildObject). Both fixtures below are REAL
// captured bytes (2 confirmed sightings each this session, byte-identical
// both times) for a real "Cell 1" room inside a real building (the same
// "Travel Terminal" TangibleObject already pinned in
// test_swgproto_tangibleobject.cpp/test_swgproto_weaponobject.cpp).
//
// Both fixtures directly settle a place where Core3's OWN source comments
// disagreed with its current compiled code:
// - CellObjectMessage3.h's trailing comment claims a live capture had an
//   extra unaccounted byte right before cellNumber (making the payload 25
//   bytes) - the real fixture below is 24 bytes, matching the CURRENT
//   constructor's 7 insert() calls exactly, with zero leftover. That old
//   comment was stale/wrong.
// - CellObjectMessage6.h's comments show three different historical
//   values for the leading field (0x42, 0x8C, 0x95 across old commented
//   code and a stale "Pre-CU"/"Current Live" dump) - the real fixture
//   below is 0x95 (149), matching the CURRENT compiled constructor, not
//   either stale value.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CellObjectBaseline3.h"
#include "swgproto/CellObjectBaseline6.h"
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

TEST_CASE("CellObjectBaseline3::parse - real payload (Cell 1, Travel Terminal)") {
    auto buf = bufferFromHex(
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00");

    auto result = CellObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownField0 == 0);
    CHECK(b.stfNameStringIndex == 0);
    CHECK(b.unknownField1 == 0);
    CHECK(b.stfStringIndex == 0);
    CHECK(b.customName.empty());
    CHECK(b.unknownField2 == 0);
    CHECK(b.cellNumber == 1);

    CHECK(buf.remaining() == 0);
}

TEST_CASE("CellObjectBaseline6::parse - real payload (Cell 1, Travel Terminal)") {
    auto buf = bufferFromHex("95 00 00 00 00 00 00 00 00 00 00 00");

    auto result = CellObjectBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownConst == 0x95);
    CHECK(b.unknownList.empty());

    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - CellObject BASE3 cellNumber (0x05)") {
    // Synthetic (not a real capture): field index 0x05 (cellNumber) is the
    // ONLY delta setter CellObjectDeltaMessage3.h exposes - a regression
    // guard on that specific wiring. Note the index (5) does NOT match
    // cellNumber's position (7th/last) in this struct's own field array -
    // it's Core3's own opaque bookkeeping constant, confirmed directly
    // from CellObjectDeltaMessage3::updateCellNumber()'s addIntUpdate(5,
    // ...) call, same as every other delta index in this project.
    std::vector<uint8_t> bytes = {0x05, 0x00, 0x02, 0x00, 0x00, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kCellObjectBaseline3Schema, 1, buf);

    CHECK_FALSE(decoded.stoppedEarly);
    REQUIRE(decoded.updates.size() == 1);
    CHECK(decoded.updates[0].fieldIndex == 0x05);
    CHECK(decoded.updates[0].fieldName == "cellNumber");
    CHECK(decoded.updates[0].valueText == "2");
}
