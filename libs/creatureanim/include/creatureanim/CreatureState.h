#pragma once

#include <cstdint>
#include <string>

namespace creatureanim {

// Real Core3 `CreatureState` bitmask values (`CreatureObjectBaseline3::
// stateBitmask`, wire index 0x10) - confirmed directly against Core3's own
// `templates/params/creature/CreatureState.h` (live-read from a real
// running server, not guessed). Per this project's own "Core3 is a map,
// not truth" discipline: this one is a deliberate exception, not a
// contradiction of it - the discipline exists because Core3's PROTOCOL
// decoding (message formats, field order) is easy to get subtly wrong by
// copying without verifying against real captured bytes. `stateBitmask`
// isn't a format this project decodes independently - it's a raw uint64
// the SERVER itself sets and sends verbatim, so there's no independent
// wire-format question to get wrong; the only real question is "what do
// the bits mean," which Core3's own source answers directly and
// authoritatively (Core3 IS the server that sets these bits).
enum class CreatureStateBit : uint64_t {
    Cover = 0x01,
    Combat = 0x02,
    Peace = 0x04,
    Aiming = 0x08,
    Alert = 0x10,
    Berserk = 0x20,
    FeignDeath = 0x40,
    CombatAttitudeEvasive = 0x80,
    CombatAttitudeNormal = 0x100,
    CombatAttitudeAggressive = 0x200,
    Tumbling = 0x400,
    Rallied = 0x800,
    Stunned = 0x1000,
    Blinded = 0x2000,
    Dizzy = 0x4000,
    Intimidated = 0x8000,
    Immobilized = 0x10000,
    Frozen = 0x20000,
    Swimming = 0x40000,
    SittingOnChair = 0x80000,
    Crafting = 0x100000,
    GlowingJedi = 0x200000,
    MaskScent = 0x400000,
    Poisoned = 0x800000,
    Bleeding = 0x1000000,
    Diseased = 0x2000000,
    OnFire = 0x4000000,
    RidingMount = 0x8000000,       // riding a vehicle or creature mount
    MountedCreature = 0x10000000,  // IS a vehicle/creature mount that has a rider
    PilotingShip = 0x20000000,
    ShipOperations = 0x40000000,
    ShipGunner = 0x80000000,
    ShipInterior = 0x100000000,
    PilotingPobShip = 0x200000000,
};

bool hasState(uint64_t stateBitmask, CreatureStateBit bit);

// Real bit names, matching Core3's own `state.iff` datatable naming
// (lowercase) - for diagnostic logging, not consumed by selection logic.
// Returns every set bit's name joined with "+", or "none" if stateBitmask
// is 0. Unknown bits not in the enum above print as their own raw hex
// value rather than being silently dropped.
std::string describeStateBitmask(uint64_t stateBitmask);

} // namespace creatureanim
