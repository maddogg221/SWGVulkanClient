// Permanent regression test for ResourceContainer BASE3/6 (Phase 6 Tier 2
// remainder, unblocked by CommandQueueEnqueue - see PLAN.md). Real fixtures
// captured live against Naritus 2026-07-17 via
// `/object createresource Klebe 500` (an admin-spawned 500-unit stack of the
// then-active Naboo resource "Klebe", underlying type
// "wheat_domesticated_naboo") - the first Phase 6 item to use the new
// command-send capability rather than passive observation. Both fixtures
// independently decoded via a from-scratch Node.js script before being
// pinned here (this project's established cross-check discipline), zero
// leftover bytes either way.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/ResourceContainerBaseline3.h"
#include "swgproto/ResourceContainerBaseline6.h"
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

TEST_CASE("ResourceContainerBaseline3::parse - real Naritus fixture (500 Klebe)") {
    auto buf = bufferFromHex(
        "00 00 c8 42 14 00 72 65 73 6f 75 72 63 65 5f 63 6f 6e 74 61 69 6e 65 72 5f 6e 00 00 "
        "00 00 12 00 6f 72 67 61 6e 69 63 5f 66 6f 6f 64 5f 73 6d 61 6c 6c 12 00 00 00 44 00 "
        "6f 00 6d 00 65 00 73 00 74 00 69 00 63 00 61 00 74 00 65 00 64 00 20 00 57 00 68 00 "
        "65 00 61 00 74 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 f4 01 00 00 "
        "00 00 00 00 e8 03 00 00 01 f4 01 00 00 2b 3f 00 01 00 00 14 00");

    auto result = ResourceContainerBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.tangible.complexity == doctest::Approx(100.0f));
    CHECK(b.tangible.objectName.file == "resource_container_n");
    CHECK(b.tangible.objectName.stringId == "organic_food_small");
    CHECK(b.tangible.customObjectName == u"Domesticated Wheat");
    CHECK(b.tangible.useCount == 500);
    CHECK(b.tangible.maxCondition == 1000);
    CHECK(b.quantity == 500);
    CHECK(b.resourceId == 5629499551006507ULL);

    CHECK(buf.remaining() == 0);
}

TEST_CASE("ResourceContainerBaseline6::parse - real Naritus fixture (Klebe)") {
    auto buf = bufferFromHex(
        "00 00 00 00 00 00 00 00 00 00 00 00 a0 86 01 00 18 00 77 68 65 61 74 5f 64 6f 6d 65 "
        "73 74 69 63 61 74 65 64 5f 6e 61 62 6f 6f 05 00 00 00 4b 00 6c 00 65 00 62 00 65 00");

    auto result = ResourceContainerBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownField0.empty());
    CHECK(b.unknownField1 == 0);
    CHECK(b.unknownField2.empty());
    CHECK(b.containerName.empty());
    CHECK(b.maxStackSize == 100000);
    CHECK(b.spawnType == "wheat_domesticated_naboo");
    CHECK(b.resourceName == u"Klebe");

    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - ResourceContainer BASE3 quantity (0x0B)") {
    // Synthetic (not a real capture - no live quantity-change delta was
    // observed this session): field index 0x0B (quantity) is confirmed live
    // from ResourceContainerObjectDeltaMessage3.h's updateQuantity(), and IS
    // actually called by setQuantity()'s broadcast - a regression guard on
    // that wiring, same "simple + literal-confirmed, not yet observed"
    // precedent as InstallationObject's activeFlag.
    std::vector<uint8_t> bytes = {0x0B, 0x00, 0xF4, 0x01, 0x00, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kResourceContainerBaseline3Schema, 1, buf);

    CHECK_FALSE(decoded.stoppedEarly);
    REQUIRE(decoded.updates.size() == 1);
    CHECK(decoded.updates[0].fieldIndex == 0x0B);
    CHECK(decoded.updates[0].fieldName == "quantity");
    CHECK(decoded.updates[0].valueText == "500");
}

TEST_CASE("decodeDeltaMessage - ResourceContainer BASE3 stops at unmapped resourceId index") {
    // resourceId has no confirmed live delta path (the delta message's own
    // setResourceID(0x0E) is commented-out, uncalled dead code) - pins that
    // a delta claiming that index is correctly rejected, not misdecoded.
    std::vector<uint8_t> bytes = {0x0E, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kResourceContainerBaseline3Schema, 1, buf);

    CHECK(decoded.stoppedEarly);
    CHECK(decoded.updates.empty());
}
