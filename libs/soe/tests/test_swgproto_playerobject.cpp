// Permanent regression tests for the Phase 2 step 5 PlayerObject BASE3/
// BASE6 parsers, mirroring test_swgproto_creatureobject.cpp's convention:
// real byte-for-byte payloads captured from a live Finalizer session
// (character "Kalda Ulzo", the self player's PlayerObject "ghost" object)
// via a temporary hex-dump patch, not synthetic/hand-built buffers. The
// delta-decode cases ARE synthetic here too, even though real self-deltas
// did arrive live this session (see DISCOVERY.txt) - captured console
// output, not raw bytes, so there was nothing to re-derive a byte fixture
// from without another capture pass.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/PlayerObjectBaseline3.h"
#include "swgproto/PlayerObjectBaseline6.h"
#include "swgproto/PlayerObjectDelta.h"

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

TEST_CASE("PlayerObjectBaseline3::parse - real BASE3 payload (Kalda Ulzo ghost, Finalizer)") {
    // Captured 2026-07-15 via a temporary hex-dump patch on the same
    // character's zone-in used for the CreatureObject fixtures. 86 bytes,
    // independently re-verified field-by-field (complexity==1.0f exactly,
    // matching source's "always literal 1" comment; empty name/title;
    // playerBitmask[0]==256; unknownTail matching the 0x6C2/0xDC62/0x23
    // literals from source) before being pinned here.
    auto buf = bufferFromHex(
        "00 00 80 3f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 04 00 00 00 "
        "00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 43 7f f8 67 26 03 00 00 c2 06 00 00 62 dc 00 00 23 00 "
        "00 00");

    auto result = PlayerObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& p = result.value();
    const auto& it = p.intangible;

    CHECK(it.complexity == doctest::Approx(1.0f));
    CHECK(it.objectName.file.empty());
    CHECK(it.objectName.stringId.empty());
    CHECK(it.customObjectName.empty());
    CHECK(it.volume == 0);
    CHECK(it.status == 0);

    CHECK(p.playerBitmask == std::array<int32_t, 4>{256, 0, 0, 0});
    CHECK(p.profileSettings == std::array<int32_t, 4>{0, 0, 0, 0});
    CHECK(p.title.empty());
    CHECK(p.birthDate == 1744338755);
    CHECK(p.totalPlayedTime == 806);
    CHECK(p.unknownTail == std::array<int32_t, 3>{1730, 56418, 35});
    CHECK(buf.remaining() == 0);
}

TEST_CASE("PlayerObjectBaseline6::parse - real BASE6 payload (Kalda Ulzo ghost, Finalizer)") {
    // Captured 2026-07-15, same session. 5 bytes - this is itself the
    // regression fixture proving the opcnt=3-but-only-2-fields resolution
    // from Stage 2 (buf.remaining() == 0 after 2 fields, confirmed live).
    auto buf = bufferFromHex("00 00 00 00 00");

    auto result = PlayerObjectBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& p = result.value();

    CHECK(p.unknown == 0);
    CHECK(p.privFlag == 0);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodePlayerObjectDelta - BASE3 fixed-4-int and scalar fields") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0005); // playerBitmask
    buf.writeUint32(4);      // literal tag, not count+updateCounter
    buf.writeUint32(1);
    buf.writeUint32(2);
    buf.writeUint32(3);
    buf.writeUint32(4);
    buf.writeUint16(0x0008); // birthDate
    buf.writeUint32(1700000000);

    auto result = decodePlayerObjectDelta(3, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "playerBitmask");
    CHECK(result.updates[0].valueText == "[1, 2, 3, 4]");
    CHECK(result.updates[1].fieldName == "birthDate");
    CHECK(result.updates[1].valueText == "1700000000");
}

TEST_CASE("decodePlayerObjectDelta - BASE6 scalar fields") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0001); // privFlag
    buf.writeByte(5);

    auto result = decodePlayerObjectDelta(6, 1, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "privFlag");
    CHECK(result.updates[0].valueText == "5");
}

TEST_CASE("decodePlayerObjectDelta - stops at unmapped field index") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0001); // privFlag
    buf.writeByte(1);
    buf.writeUint16(0x0002); // unmapped for BASE6 - confirmed by live traffic not to exist
    buf.writeUint32(0);

    auto result = decodePlayerObjectDelta(6, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "privFlag");
}

TEST_CASE("decodePlayerObjectDelta - stops if fixed-4-int tag isn't 4") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0006); // profileSettings
    buf.writeUint32(7);      // wrong tag - should always be 4 per source

    auto result = decodePlayerObjectDelta(3, 1, buf);
    CHECK(result.stoppedEarly);
    CHECK(result.updates.empty());
}
