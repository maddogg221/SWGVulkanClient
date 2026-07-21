#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's CombatSpam.h:
// StandaloneObjectControllerMessage(receiver->getObjectID(), 0x1B, 0x134).
constexpr uint32_t kCombatSpamControllerType = 0x134;

// Combat feedback text (e.g. "%TU hits %TT for %DI damage"). CONFIRMED
// DEAD CODE on both known servers (Phase 4 Step 3/5 - zero real traffic
// across ~30,000+ captured packets spanning both taking and dealing lethal
// damage; this server's real combat feedback runs through ShowFlyText +
// CreatureObject deltas instead). SYNTHETIC TEST ONLY, same treatment as
// CombatAction (both deferred/synthetic - see KNOWN_UNKNOWNS.md). Unlike
// CombatAction, this message's shape has zero ambiguity: CombatSpam.h's
// two constructor overloads converge to the exact same wire layout (the
// "custom message" overload's insertUnicode(uniString) and the "templated
// string" overload's insertInt(0) produce byte-identical output when the
// unicode string is empty, since insertUnicode's own encoding IS a
// uint32 count prefix + that many uint16 chars).
struct CombatSpam {
    uint64_t attackerId = 0;
    uint64_t defenderId = 0;
    uint64_t itemId = 0;
    uint32_t damage = 0;
    std::string file;
    // Always 0 in source ("//padding") - kept as a real (if inert) field
    // for fidelity rather than assumed and skipped.
    uint32_t padding = 0;
    std::string stringName;
    uint8_t color = 0;
    std::u16string message;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static CombatSpam parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
