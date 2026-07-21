#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/DeltaFieldUpdate.h"

namespace swgproto {

// Result of decoding a BASE1/BASE3/BASE6 DeltasMessage's `count` update
// entries against the known field-index table (delta field index == the
// same flat position CreatureObjectBaseline1/3/6 read their fields at -
// confirmed via CreatureObjectDeltaMessage1.h/DeltaMessage3.h/
// DeltaMessage6.h). Every KNOWN field is decoded or skipped and decoding
// continues to the next update; only a field index outside the known table
// (or a buffer underflow) halts decoding early, since its width can't be
// determined safely - see `stoppedEarly`/`stopReason`.
struct CreatureObjectDeltaDecode {
    std::vector<DeltaFieldUpdate> updates;
    uint16_t totalCount = 0; // the envelope's own `count` - may be > updates.size()
    bool stoppedEarly = false;
    std::string stopReason; // populated iff stoppedEarly
};

// `baselineNumber` must be 1, 3, 4, or 6 (the only baselines this project has
// a field-index table for so far). Reads entries from `buf` until `count` is
// reached or decoding stops early.
CreatureObjectDeltaDecode decodeCreatureObjectDelta(uint8_t baselineNumber, uint16_t count,
                                                     soe::PacketBuffer& buf);

} // namespace swgproto
