// Permanent regression test for HarvesterObjectBaseline7/HarvesterObjectDelta7
// (Phase 9). Real captured fixtures - a moisture vaporator placed on Naboo
// (objectId 562949970715735) via /placestructure, queried via
// /harvesterGetResourceData + /harvesterSelectResource + /harvesterActivate
// + /synchronizeduilisten.
//
// The first capture (below) is INACTIVE-STATE: activation genuinely never
// flipped `isActive` to true across many real attempts, even with real
// maintenance/power funded - eventually root-caused (see KNOWN_UNKNOWNS.md's
// "HarvesterObject BASE7" entry, DISCOVERY.txt's Phase 9 entry) to
// `StructureDeed::extractionRate`/`hopperSizeMax` both defaulting to 0 in
// source and only ever getting populated by the real crafting pipeline -
// this project's admin-spawned deed (`/object createitem`, bypassing
// crafting entirely) never received real values, so the harvester's very
// first periodic work-tick saw hopperSize(0) >= hopperSizeMax(0) and
// immediately auto-shut-down again, every time. Not a Core3 bug - a direct,
// fully-understood consequence of the admin-spawn shortcut. Kept as a
// permanent fixture since it exercises every field's real byte position
// with faithfully-zero dynamic values, not a simplification.
//
// The second capture (real ACTIVE-state baseline + two real deltas) was
// obtained by temporarily patching the live test server to force realistic
// `hopperSizeMax`/`extractionRate` values at activation time (bypassing the
// missing-crafting gap for investigation purposes only - reverted
// afterward), then holding a long-lived connection open with periodic
// `/synchronizeduilisten` re-syncs to observe real accumulation.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/HarvesterObjectBaseline7.h"
#include "swgproto/HarvesterObjectDelta7.h"

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

TEST_CASE("HarvesterObjectBaseline7::parse - real inactive-state capture") {
    auto buf = bufferFromHex(
        "01 02 00 00 00 02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 "
        "02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 02 00 00 00 05 "
        "00 45 6a 6f 67 61 03 00 4f 63 69 02 00 00 00 02 00 00 00 11 00 77 61 74 65 72 5f 76 61 "
        "70 6f 72 5f 6e 61 62 6f 6f 11 00 77 61 74 65 72 5f 76 61 70 6f 72 5f 6e 61 62 6f 6f 2a "
        "3f 00 01 00 00 14 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 02 "
        "01 00 00 00 02 00 00 00 2a 3f 00 01 00 00 14 00 00 00 00 00 64");

    auto result = HarvesterObjectBaseline7::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownFlag == 1);

    REQUIRE(b.resourceIdList1.size() == 2);
    CHECK(b.resourceIdList1[0] == 5629499551006506ULL);
    CHECK(b.resourceIdList1[1] == 5629499551006214ULL);
    // Byte-identical duplicate, confirmed live - not a transcription mistake.
    CHECK(b.resourceIdList2 == b.resourceIdList1);

    REQUIRE(b.resourceNameList.size() == 2);
    CHECK(b.resourceNameList[0] == "Ejoga");
    CHECK(b.resourceNameList[1] == "Oci");

    REQUIRE(b.resourceTypeList.size() == 2);
    CHECK(b.resourceTypeList[0] == "water_vapor_naboo");
    CHECK(b.resourceTypeList[1] == "water_vapor_naboo");

    CHECK(b.activeResourceSpawnId == 5629499551006506ULL);
    CHECK(b.isActive == false);
    CHECK(b.extractionRateDisplayed == 0);
    CHECK(b.extractionRateMax == doctest::Approx(0.0f));
    CHECK(b.actualExtractRate == doctest::Approx(0.0f));
    CHECK(b.hopperSize == doctest::Approx(0.0f));
    CHECK(b.hopperSizeMax == 0);

    // hopperList has exactly 1 entry (the selected resource, pre-registered
    // at 0 quantity) - hopperLeadByte == size+1 == 2, the documented quirk,
    // confirmed live.
    CHECK(b.hopperLeadByte == 2);
    REQUIRE(b.hopperList.size() == 1);
    CHECK(b.hopperList[0].resourceSpawnId == 5629499551006506ULL);
    CHECK(b.hopperList[0].quantity == doctest::Approx(0.0f));

    CHECK(b.condition == 100);

    CHECK(buf.remaining() == 0);
}

TEST_CASE("HarvesterObjectBaseline7::parse - real ACTIVE-state capture") {
    auto buf = bufferFromHex(
        "01 02 00 00 00 02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 "
        "02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 02 00 00 00 05 "
        "00 45 6a 6f 67 61 03 00 4f 63 69 02 00 00 00 02 00 00 00 11 00 77 61 74 65 72 5f 76 61 "
        "70 6f 72 5f 6e 61 62 6f 6f 11 00 77 61 74 65 72 5f 76 61 70 6f 72 5f 6e 61 62 6f 6f 2a "
        "3f 00 01 00 00 14 00 01 0a 00 00 00 00 00 20 41 6e bd eb 3f 00 00 00 40 10 27 00 00 02 "
        "01 00 00 00 0f 00 00 00 2a 3f 00 01 00 00 14 00 00 00 00 40 64");

    auto result = HarvesterObjectBaseline7::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.activeResourceSpawnId == 5629499551006506ULL);
    CHECK(b.isActive == true);
    CHECK(b.extractionRateDisplayed == 10);
    CHECK(b.extractionRateMax == doctest::Approx(10.0f));
    // actualRate == spawnDensity * extractionRateMax (0.184172 * 10) - the
    // real formula, confirmed live.
    CHECK(b.actualExtractRate == doctest::Approx(1.8417184f));
    CHECK(b.hopperSize == doctest::Approx(2.0f));
    CHECK(b.hopperSizeMax == 10000);

    REQUIRE(b.hopperList.size() == 1);
    CHECK(b.hopperList[0].resourceSpawnId == 5629499551006506ULL);
    CHECK(b.hopperList[0].quantity == doctest::Approx(2.0f));

    CHECK(buf.remaining() == 0);
}

TEST_CASE("applyHarvesterObjectBaseline7Delta - two real captured deltas, hopper accumulating") {
    // First real delta (40 bytes, zero leftover): hopper quantity 0 -> 5.
    auto buf1 = bufferFromHex("0c 00 01 0d 00 01 00 00 00 11 00 00 00 02 00 00 2a 3f 00 01 00 00 "
                               "14 00 00 00 a0 40 0a 00 00 00 a0 40 09 00 6e bd eb 3f");

    HarvesterObjectBaseline7 state;
    state.hopperList.push_back({5629499551006506ULL, 0.0f});

    auto result1 = applyHarvesterObjectBaseline7Delta(state, 4, buf1);
    CHECK_FALSE(result1.stoppedEarly);
    CHECK(result1.appliedFieldIndices.size() == 4);
    CHECK(buf1.remaining() == 0);

    REQUIRE(state.hopperList.size() == 1);
    CHECK(state.hopperList[0].resourceSpawnId == 5629499551006506ULL);
    CHECK(state.hopperList[0].quantity == doctest::Approx(5.0f));
    CHECK(state.hopperSize == doctest::Approx(5.0f));
    CHECK(state.actualExtractRate == doctest::Approx(1.8417184f));

    // Second real delta (captured moments later): hopper quantity 5 -> 6,
    // real accumulation over real elapsed time.
    auto buf2 = bufferFromHex("0c 00 01 0d 00 01 00 00 00 12 00 00 00 02 00 00 2a 3f 00 01 00 00 "
                               "14 00 00 00 c0 40 0a 00 00 00 c0 40 09 00 6e bd eb 3f");

    auto result2 = applyHarvesterObjectBaseline7Delta(state, 4, buf2);
    CHECK_FALSE(result2.stoppedEarly);
    CHECK(buf2.remaining() == 0);

    REQUIRE(state.hopperList.size() == 1);
    CHECK(state.hopperList[0].quantity == doctest::Approx(6.0f));
    CHECK(state.hopperSize == doctest::Approx(6.0f));
}

TEST_CASE("applyHarvesterObjectBaseline7Delta - stops early on an unmapped field index") {
    std::vector<uint8_t> bytes = {0x03, 0x00}; // index 0x03 has no confirmed live shape
    soe::PacketBuffer buf(bytes.data(), bytes.size());

    HarvesterObjectBaseline7 state;
    auto result = applyHarvesterObjectBaseline7Delta(state, 1, buf);

    CHECK(result.stoppedEarly);
    CHECK(result.appliedFieldIndices.empty());
}
