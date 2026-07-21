#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/HarvesterObjectBaseline7.h"

namespace swgproto {

// Result of applying a BASE7 delta's `count` update entries onto an
// already-decoded HarvesterObjectBaseline7.
struct HarvesterObjectDeltaResult {
    std::vector<uint16_t> appliedFieldIndices;
    uint16_t totalCount = 0;
    bool stoppedEarly = false;
    std::string stopReason; // populated iff stoppedEarly
};

// Applies `count` field updates from `buf` onto `state` in place. Sent
// under a DIFFERENT tag than its own baseline: "INSO" (0x494E534F), not
// "HINO" (0x48494E4F) - HarvesterObject reuses InstallationObjectDeltaMessage7
// unmodified (no dedicated HarvesterObjectDeltaMessage7 class exists in
// source). Confirmed live indices, from two real captured deltas (40 bytes
// each, zero leftover, both matching the documented field order exactly -
// captured via a long-lived watch session after a real, fully-active
// harvester, following an admin-generated-deed hopperSizeMax/extractionRate
// fix - see PLAN.md's Phase 9 entry for the full story):
//   0x05 updateActiveResourceSpawn(uint64) - documented live in source, not
//        yet independently observed on the wire
//   0x06 updateOperating(bool) - documented live in source, not yet
//        independently observed on the wire
//   0x09 updateExtractionRate(float) - CONFIRMED live (the "actual" rate,
//        equal to spawnDensity * extractionRateMax in both real captures)
//   0x0A updateHopperSize(float) - CONFIRMED live, matches the hopper
//        entry's own quantity exactly in both real captures
//   0x0C updateHopper - CONFIRMED live, a scalar byte flag (always 1 in
//        both real captures) sent immediately before 0x0D; not separately
//        modeled on HarvesterObjectBaseline7 (purely a "hopper changed"
//        notification, no persisted meaning beyond that)
//   0x0D hopperList - CONFIRMED live, the same nested list-command shape
//        as GroupObject's member/ship lists but with GroupObject's own tag
//        scheme (ADD=1/SET=2/REMOVE=0/CLEAR=4) - both real captures used
//        tag=2 (SET) to update the single existing hopper entry's quantity
//        (5.0 -> 6.0 between the two captures, real accumulation over
//        real elapsed time).
// Deliberately NOT built on SchemaEngine::applyDeltaMessage, for the same
// reason as GroupObjectDelta6: 0x0D incrementally mutates an existing list
// via per-entry tags rather than replacing one field with one freshly-
// decoded value - a structurally different contract than every FieldKind
// relies on.
HarvesterObjectDeltaResult applyHarvesterObjectBaseline7Delta(HarvesterObjectBaseline7& state,
                                                                uint16_t count, soe::PacketBuffer& buf);

} // namespace swgproto
