#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

constexpr uint32_t kShowFlyTextControllerType = 0x1BD;

// Floating combat/status text shown above an object - e.g. hit-location
// feedback ("combat_effects"/"hit_head") or generic NPC reactions
// ("npc_reaction/flytext"/"alert"). Confirmed via Core3's ShowFlyText.h:
// ObjectControllerMessage(creo->getObjectID(), 0x1B, 0x1BD), plus the
// constructor's own insert calls for the fields below. `file`/`entry` are a
// StringId pair (a string-table filename + key within it, resolved
// client-side against the game's localization tables) - this project
// doesn't have those tables, so both are decoded as their raw ASCII names,
// not resolved to display text. Wire layout independently confirmed against
// 3 real captured payloads during Phase 4 step 3's combat capture (46/49/52
// bytes depending on string lengths, zero leftover bytes each time) -
// notably this is the channel that carries this server's real combat
// hit-location feedback, since it never sends CombatAction/CombatSpam (see
// DISCOVERY.txt's "PHASE 4 STEP 3 COMPLETE").
struct ShowFlyText {
    uint64_t targetObjectId = 0; // per Core3's own comment: "Target object ID"
    std::string file;            // StringId file, e.g. "combat_effects"
    uint32_t spacer = 0;         // always 0 in every real sample seen so far
    std::string entry;           // StringId entry, e.g. "hit_head"
    float scale = 0.0f;          // 1.0 for broadcasted text, 0 for none, per Core3's comment
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    // Bitmask, per Core3's own (partially-speculative) comment: 0x01 =
    // shown only on target, 0x02 = also shown in chat box, 0x04 = unknown.
    // Every real sample seen so far is a literal 5 (0x01 | 0x04).
    uint8_t flags = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed). Wire layout:
    // uint64 targetObjectId + ASCII file + uint32 spacer + ASCII entry +
    // float scale + byte red + byte green + byte blue + byte flags.
    static ShowFlyText parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
