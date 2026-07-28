#include "creatureanim/CreatureState.h"

#include <array>
#include <sstream>
#include <utility>

namespace creatureanim {

bool hasState(uint64_t stateBitmask, CreatureStateBit bit) {
    return (stateBitmask & static_cast<uint64_t>(bit)) != 0;
}

namespace {
constexpr std::pair<CreatureStateBit, const char*> kKnownBits[] = {
    {CreatureStateBit::Cover, "cover"},
    {CreatureStateBit::Combat, "combat"},
    {CreatureStateBit::Peace, "peace"},
    {CreatureStateBit::Aiming, "aiming"},
    {CreatureStateBit::Alert, "alert"},
    {CreatureStateBit::Berserk, "berserk"},
    {CreatureStateBit::FeignDeath, "feigndeath"},
    {CreatureStateBit::CombatAttitudeEvasive, "combatattitudeevasive"},
    {CreatureStateBit::CombatAttitudeNormal, "combatattitudenormal"},
    {CreatureStateBit::CombatAttitudeAggressive, "combatattitudeaggressive"},
    {CreatureStateBit::Tumbling, "tumbling"},
    {CreatureStateBit::Rallied, "rallied"},
    {CreatureStateBit::Stunned, "stunned"},
    {CreatureStateBit::Blinded, "blinded"},
    {CreatureStateBit::Dizzy, "dizzy"},
    {CreatureStateBit::Intimidated, "intimidated"},
    {CreatureStateBit::Immobilized, "immobilized"},
    {CreatureStateBit::Frozen, "frozen"},
    {CreatureStateBit::Swimming, "swimming"},
    {CreatureStateBit::SittingOnChair, "sittingonchair"},
    {CreatureStateBit::Crafting, "crafting"},
    {CreatureStateBit::GlowingJedi, "glowingjedi"},
    {CreatureStateBit::MaskScent, "maskscent"},
    {CreatureStateBit::Poisoned, "poisoned"},
    {CreatureStateBit::Bleeding, "bleeding"},
    {CreatureStateBit::Diseased, "diseased"},
    {CreatureStateBit::OnFire, "onfire"},
    {CreatureStateBit::RidingMount, "ridingmount"},
    {CreatureStateBit::MountedCreature, "mountedcreature"},
    {CreatureStateBit::PilotingShip, "pilotingship"},
    {CreatureStateBit::ShipOperations, "shipoperations"},
    {CreatureStateBit::ShipGunner, "shipgunner"},
    {CreatureStateBit::ShipInterior, "shipinterior"},
    {CreatureStateBit::PilotingPobShip, "pilotingpobship"},
};
} // namespace

std::string describeStateBitmask(uint64_t stateBitmask) {
    if (stateBitmask == 0) {
        return "none";
    }
    uint64_t remaining = stateBitmask;
    std::ostringstream out;
    bool first = true;
    for (const auto& [bit, name] : kKnownBits) {
        if (hasState(stateBitmask, bit)) {
            if (!first) out << "+";
            out << name;
            first = false;
            remaining &= ~static_cast<uint64_t>(bit);
        }
    }
    if (remaining != 0) {
        if (!first) out << "+";
        out << "0x" << std::hex << remaining;
    }
    return out.str();
}

} // namespace creatureanim
