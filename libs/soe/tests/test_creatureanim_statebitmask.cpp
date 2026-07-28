// Permanent regression test for creatureanim's real CreatureState bitmask
// decode - values confirmed directly against Core3's own
// templates/params/creature/CreatureState.h (live-read from a real running
// server, see PHASE_21_STATUS.md/project memory for how). This is server-
// authoritative raw data (the server itself sets these bits), not a wire
// FORMAT this project decodes independently - so unlike most fixtures in
// this suite, there's no captured-byte round-trip to pin, just the real bit
// values and the helper functions built on them.
#include <doctest/doctest.h>

#include "creatureanim/CreatureState.h"

using creatureanim::CreatureStateBit;
using creatureanim::describeStateBitmask;
using creatureanim::hasState;

TEST_CASE("creatureanim::hasState - single real bits") {
    CHECK(hasState(0x02, CreatureStateBit::Combat));
    CHECK_FALSE(hasState(0x02, CreatureStateBit::Peace));
    CHECK(hasState(0x40000, CreatureStateBit::Swimming));
    CHECK_FALSE(hasState(0, CreatureStateBit::Combat));
}

TEST_CASE("creatureanim::hasState - combined real bitmask") {
    // Combat + Aiming + Swimming, a real plausible combination.
    uint64_t combined =
        static_cast<uint64_t>(CreatureStateBit::Combat) | static_cast<uint64_t>(CreatureStateBit::Aiming) |
        static_cast<uint64_t>(CreatureStateBit::Swimming);
    CHECK(hasState(combined, CreatureStateBit::Combat));
    CHECK(hasState(combined, CreatureStateBit::Aiming));
    CHECK(hasState(combined, CreatureStateBit::Swimming));
    CHECK_FALSE(hasState(combined, CreatureStateBit::Peace));
    CHECK_FALSE(hasState(combined, CreatureStateBit::RidingMount));
}

TEST_CASE("creatureanim::describeStateBitmask - zero, single, multiple, unknown bits") {
    CHECK(describeStateBitmask(0) == "none");
    CHECK(describeStateBitmask(static_cast<uint64_t>(CreatureStateBit::Combat)) == "combat");
    CHECK(describeStateBitmask(static_cast<uint64_t>(CreatureStateBit::Cover) |
                                static_cast<uint64_t>(CreatureStateBit::Combat)) == "cover+combat");
    // A bit not in the known real enum (well past PilotingPobShip) should
    // still show up, not be silently dropped.
    std::string desc = describeStateBitmask(static_cast<uint64_t>(CreatureStateBit::Combat) | 0x400000000ull);
    CHECK(desc.find("combat") != std::string::npos);
    CHECK(desc.find("0x400000000") != std::string::npos);
}
