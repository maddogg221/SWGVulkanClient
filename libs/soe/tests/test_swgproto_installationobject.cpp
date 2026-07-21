// Permanent regression test for InstallationObject BASE3 (Phase 6 Tier 2).
// Unlike every other real-fixture test so far, NO real INSO-tagged capture
// was available this session - the test character's starting area (a
// starport) has no harvesters/factories/turrets/minefields nearby, and a
// live probe confirmed zero INSO baseline/delta traffic in that area. Per
// user direction, implementation proceeded from source alone (source
// evidence is unusually strong: InstallationObjectImplementation::
// sendBaselinesTo() sends unconditionally, and setActive()'s activeFlag
// delta update goes through an explicit broadcastMessages() call, not a
// self-only sendMessage() - see InstallationObjectBaseline3.h for the full
// citation).
//
// This test is honest about what it can and can't prove: the ANCESTOR
// portion (TangibleObjectBaseline3's own 11 fields, bytes 0-134) is the
// EXACT REAL fixture from test_swgproto_weaponobject.cpp (self's own real
// weapon, captured 2026-07-16) - already independently proven correct byte-
// for-byte. Only the trailing 9 bytes (InstallationObjectBaseline3's own 3
// fields: activeFlag/surplusPower/basePowerRate) are SYNTHETIC placeholder
// values, chosen to be easy to eyeball-verify (activeFlag=true,
// surplusPower=100.0, basePowerRate=2.5) - there is no real captured INSO
// payload to pin instead. This still meaningfully exercises the real thing
// this step adds: the ObjectSchema ancestor/ancestorOffset composition
// wiring a THIRD own field onto TangibleObjectBaseline3 correctly, and the
// exact byte offset where InstallationObject's own fields begin.
#include <doctest/doctest.h>

#include <cstring>
#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/InstallationObjectBaseline3.h"
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

TEST_CASE("InstallationObjectBaseline3::parse - real TANO ancestor + synthetic own fields") {
    auto buf = bufferFromHex(
        // Real WeaponObject/TangibleObject BASE3 fixture (135 bytes, self's
        // own weapon) - see test_swgproto_weaponobject.cpp.
        "00 00 0c 42 0b 00 77 65 61 70 6f 6e 5f 6e 61 6d 65 00 00 00 00 0d 00 76 69 62 72 6f "
        "6b 6e 75 63 6b 6c 65 72 20 00 00 00 56 00 69 00 62 00 72 00 6f 00 20 00 4b 00 6e 00 "
        "75 00 63 00 6b 00 6c 00 65 00 72 00 20 00 7c 00 7c 00 20 00 50 00 75 00 72 00 65 00 "
        "20 00 50 00 6f 00 72 00 6b 00 20 00 41 00 72 00 6d 00 73 00 01 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 21 00 00 00 00 00 00 1e 00 00 00 56 04 00 00 01 "
        // Synthetic trailer (9 bytes): activeFlag=1, surplusPower=100.0f,
        // basePowerRate=2.5f - NOT from a real capture, see file header.
        "01 00 00 c8 42 00 00 20 40");

    auto result = InstallationObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    // Ancestor fields, cross-checked against the same assertions
    // test_swgproto_weaponobject.cpp already makes on this exact fixture.
    CHECK(b.tangible.complexity == doctest::Approx(35.0f));
    CHECK(b.tangible.customObjectName == u"Vibro Knuckler || Pure Pork Arms");
    CHECK(b.tangible.conditionDamage == 30);
    CHECK(b.tangible.maxCondition == 1110);

    // InstallationObject's own 3 fields (synthetic values).
    CHECK(b.activeFlag == true);
    CHECK(b.surplusPower == doctest::Approx(100.0f));
    CHECK(b.basePowerRate == doctest::Approx(2.5f));

    CHECK(buf.remaining() == 0);
}

TEST_CASE("decodeDeltaMessage - InstallationObject BASE3 activeFlag (0x0B)") {
    // Synthetic (not a real capture): field index 0x0B (activeFlag) is the
    // ONLY delta setter InstallationObjectDeltaMessage3.h exposes - a
    // regression guard on that specific wiring.
    std::vector<uint8_t> bytes = {0x0B, 0x00, 0x01};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kInstallationObjectBaseline3Schema, 1, buf);

    CHECK_FALSE(decoded.stoppedEarly);
    REQUIRE(decoded.updates.size() == 1);
    CHECK(decoded.updates[0].fieldIndex == 0x0B);
    CHECK(decoded.updates[0].fieldName == "activeFlag");
    CHECK(decoded.updates[0].valueText == "true");
}

TEST_CASE("decodeDeltaMessage - InstallationObject BASE3 stops at unmapped surplusPower index") {
    // surplusPower (0x0C) has no delta setter in source at all - pins that
    // a delta claiming it is correctly rejected rather than misdecoded,
    // matching this project's "detect and stop, don't guess" doctrine.
    std::vector<uint8_t> bytes = {0x0C, 0x00};
    soe::PacketBuffer buf(bytes.data(), bytes.size());
    auto decoded = decodeDeltaMessage(kInstallationObjectBaseline3Schema, 1, buf);

    CHECK(decoded.stoppedEarly);
    CHECK(decoded.updates.empty());
}
