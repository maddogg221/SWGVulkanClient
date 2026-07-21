#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/DeltaFieldUpdate.h"

namespace swgproto {

// Result of decoding a PLAY3/PLAY6 DeltasMessage's `count` update entries
// against the known field-index table (delta field index == the same flat
// position PlayerObjectBaseline3/PlayerObjectBaseline6 read their fields
// at). Same decode/skip/stop design as CreatureObjectDeltaDecode (see
// CreatureObjectDelta.h) - deliberately a separate type, not shared,
// per this project's "hand-write the second real case before
// generalizing" decision.
struct PlayerObjectDeltaDecode {
    std::vector<DeltaFieldUpdate> updates;
    uint16_t totalCount = 0; // the envelope's own `count` - may be > updates.size()
    bool stoppedEarly = false;
    std::string stopReason; // populated iff stoppedEarly
};

// `baselineNumber` must be 3 or 6 (the only PlayerObject baselines this
// project has a field-index table for so far - PLAY8/PLAY9 are deferred,
// see PLAN.md step 5's scope). Reads entries from `buf` until `count` is
// reached or decoding stops early.
PlayerObjectDeltaDecode decodePlayerObjectDelta(uint8_t baselineNumber, uint16_t count,
                                                 soe::PacketBuffer& buf);

} // namespace swgproto
