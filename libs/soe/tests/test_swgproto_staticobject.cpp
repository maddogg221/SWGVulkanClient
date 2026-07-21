// Permanent regression test for StaticObject BASE3/6 (Phase 6 Tier 3
// remainder). Real fixtures captured live against Naritus 2026-07-17 via
// `/createSpawningElement spawn object/static/structure/general/campfire_fresh.iff`
// - NOT `/object createitem`, which fails for StaticObject templates (Core3's
// own createitem command dynamic_casts the created object to TangibleObject*,
// which fails since StaticObject is an unrelated SceneObject subclass, not a
// TangibleObject - see StaticObjectBaseline3.h). Both fixtures independently
// decoded via a from-scratch Node.js script before being pinned, zero
// leftover bytes either way.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/StaticObjectBaseline3.h"
#include "swgproto/StaticObjectBaseline6.h"

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

TEST_CASE("StaticObjectBaseline3::parse - real Naritus fixture (campfire_fresh)") {
    auto buf = bufferFromHex(
        "00 00 00 00 05 00 6f 62 6a 5f 6e 00 00 00 00 0e 00 75 6e 6b 6e 6f 77 6e 5f 6f 62 6a 65 "
        "63 74 00 00 00 00 ff 00 00 00");

    auto result = StaticObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownField0 == 0);
    CHECK(b.objectName.file == "obj_n");
    CHECK(b.objectName.stringId == "unknown_object");
    CHECK(b.customObjectName.empty());
    CHECK(b.unknownField1 == 255);

    CHECK(buf.remaining() == 0);
}

TEST_CASE("StaticObjectBaseline6::parse - real Naritus fixture (campfire_fresh)") {
    auto buf = bufferFromHex(
        "44 00 00 00 0f 00 53 74 72 69 6e 67 5f 69 64 5f 74 61 62 6c 65 00 00 00 00 00 00");

    auto result = StaticObjectBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownField0 == 0x44);
    CHECK(b.literalString == "String_id_table");
    CHECK(b.unknownField1 == 0);
    CHECK(b.unknownField2 == 0);

    CHECK(buf.remaining() == 0);
}
