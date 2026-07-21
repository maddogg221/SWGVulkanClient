#pragma once

#include <cstdint>
#include <string>

namespace swgproto {

// One decoded (or skipped) field update from a DeltasMessage, produced by
// any of this project's per-object-type delta decoders (see
// CreatureObjectDelta.h, PlayerObjectDelta.h). Deliberately generic/dumb -
// just an (index, name, human-readable value) tuple - shared because it
// carries no per-object-type meaning of its own, unlike the decoders that
// produce it, which stay separate per object type on purpose (see this
// project's own notes on not merging these into a shared engine yet).
struct DeltaFieldUpdate {
    uint16_t fieldIndex = 0;
    std::string fieldName;  // e.g. "bankCredits", or "wounds" for a skipped container
    std::string valueText;  // human-readable value, or e.g. "<9 items skipped>"
};

} // namespace swgproto
