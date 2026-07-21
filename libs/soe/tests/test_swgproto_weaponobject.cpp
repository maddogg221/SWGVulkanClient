// Regression test for WeaponObject (WEAO) reusing TangibleObjectBaseline3's
// schema unchanged. Unlike every other object type so far, WeaponObject
// needed ZERO new schema code - confirmed directly from Core3 source
// (WeaponObjectMessage3.h/WeaponObjectMessage6.h both extend
// TangibleObjectMessage3/6 with no extra fields; all "extra field" code in
// WeaponObjectMessage3 is dead/commented out). No dedicated
// WeaponObjectDeltaMessage class exists either, so deltas reuse
// TangibleObjectDeltaMessage3/6 directly too. This test exists purely to
// pin down, with a REAL captured payload under the "WEAO" tag, that the
// wire format really is byte-identical to a plain TangibleObject BASE3 -
// not just assumed from source - guarding against a future regression if
// TangibleObjectBaseline3 ever changes in a way that would silently break
// WeaponObject's reuse of it.
//
// Confirmed via a live traffic probe (not a new parser, so no synthetic
// tests needed here) that WEAO is NOT self-only: 261 distinct WEAO objects
// seen in one capture, including self's own known weapon among them -
// broadcast to every weapon in the zone, same pattern as CreatureObject
// BASE6. See DISCOVERY.txt's "PHASE 6 STEP 2 COMPLETE" for full detail.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
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

TEST_CASE("TangibleObjectBaseline3::parse - real WeaponObject (WEAO) BASE3 payload") {
    // Captured 2026-07-16 via a temporary hex-dump patch, self's own
    // weapon ("Kalda Ulzo", Finalizer) - a real player-crafted item, not a
    // generic NPC default weapon, so it exercises a genuinely populated
    // customObjectName/complexity/condition rather than all-default values.
    // 135 bytes, independently re-verified (complexity/customObjectName/
    // conditionDamage/maxCondition all cross-checked against the live
    // decoded console output before being pinned) - zero leftover bytes.
    auto buf = bufferFromHex(
        "00 00 0c 42 0b 00 77 65 61 70 6f 6e 5f 6e 61 6d 65 00 00 00 00 0d 00 76 69 62 72 6f "
        "6b 6e 75 63 6b 6c 65 72 20 00 00 00 56 00 69 00 62 00 72 00 6f 00 20 00 4b 00 6e 00 "
        "75 00 63 00 6b 00 6c 00 65 00 72 00 20 00 7c 00 7c 00 20 00 50 00 75 00 72 00 65 00 "
        "20 00 50 00 6f 00 72 00 6b 00 20 00 41 00 72 00 6d 00 73 00 01 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 21 00 00 00 00 00 00 1e 00 00 00 56 04 00 00 01");

    auto result = TangibleObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& t = result.value();

    CHECK(t.complexity == doctest::Approx(35.0f));
    CHECK(t.objectName.file == "weapon_name");
    CHECK(t.objectName.stringId == "vibroknuckler");
    CHECK(t.customObjectName == u"Vibro Knuckler || Pure Pork Arms");
    CHECK(t.visibleComponents.empty());
    CHECK(t.optionsBitmask == 0x2100);
    CHECK(t.conditionDamage == 30);
    CHECK(t.maxCondition == 1110);
    CHECK(t.objectVisible == true);
    CHECK(buf.remaining() == 0);
}
