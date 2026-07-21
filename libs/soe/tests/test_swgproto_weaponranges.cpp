// Tests for WeaponRanges (Phase 4 step 7's ObjControllerMessage sub-type),
// the highest-priority undecoded type from early Phase 4 exploration
// (892 real instances observed - more than double any other undecoded type
// at the time). Confirmed live from source (WeaponRanges.h's constructor)
// that this fires from CreatureObjectImplementation::setWeapon() - NOT
// combat-specific despite the observed frequency, which reflects how often
// setWeapon() itself gets called during real play, not a periodic idle
// broadcast. Both real fixtures below were captured from a single plain
// zone-in (no combat needed) - one for the default (unarmed) weapon
// assigned at character load, one for the character's actual equipped
// weapon moments later - confirming the fixed 28-byte shape twice in the
// same session with zero leftover bytes each.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/WeaponRanges.h"

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

TEST_CASE("WeaponRanges::parse - real payload, default (unarmed) weapon at zone-in") {
    // Captured 2026-07-18 via a plain zone-in (swgcn_admin, Naritus). 28
    // bytes, zero leftover.
    auto buf = bufferFromHex("2e de 06 01 00 00 01 00 00 00 a0 40 00 00 a0 40 00 00 00 00 03 00 "
                              "00 00 05 00 00 00");

    auto msg = WeaponRanges::parse(buf);
    CHECK(msg.weaponObjectId == 281474993937966ULL);
    CHECK(msg.idealRange == doctest::Approx(5.0f));
    CHECK(msg.maxRange == doctest::Approx(5.0f));
    CHECK(msg.pointBlankAccuracy == 0);
    CHECK(msg.idealAccuracy == 3);
    CHECK(msg.maxRangeAccuracy == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("WeaponRanges::parse - real payload, actual equipped weapon (same zone-in, moments "
          "later)") {
    // Captured immediately after the default-weapon instance above, same
    // session - a genuinely different weaponObjectId (the character's real
    // equipped weapon superseding the just-assigned default). 28 bytes,
    // zero leftover.
    auto buf = bufferFromHex("39 de 06 01 00 00 01 00 00 00 40 40 00 00 a0 40 1e 00 00 00 1e 00 "
                              "00 00 1e 00 00 00");

    auto msg = WeaponRanges::parse(buf);
    CHECK(msg.weaponObjectId == 281474993937977ULL);
    CHECK(msg.idealRange == doctest::Approx(3.0f));
    CHECK(msg.maxRange == doctest::Approx(5.0f));
    CHECK(msg.pointBlankAccuracy == 30);
    CHECK(msg.idealAccuracy == 30);
    CHECK(msg.maxRangeAccuracy == 30);
    CHECK(buf.remaining() == 0);
}
