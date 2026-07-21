#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// PlayerObject's BASE6 baseline (opCount=0x03 in Core3's own
// PlayerObjectMessage6.h - standalone, does NOT extend
// IntangibleObjectMessage3). Field order confirmed directly from
// PlayerObjectMessage6.h's insert*() sequence; only field 0x01 (privFlag)
// has a confirmed delta setter (PlayerObjectDeltaMessage6.h's
// setAdminLevel). NOTE: opcnt declares 3 fields (0x00-0x02) but the
// constructor body as read from source only ever writes 2 (unknown,
// privFlag) - field 0x02's content is unaccounted for in source, and was
// confirmed BENIGN by live traffic (buf.remaining()==0 after exactly these
// 2 fields on every real capture - see DISCOVERY.txt's PHASE 2 STEP 5
// COMPLETE) rather than a hidden third field.
struct PlayerObjectBaseline6 {
    int32_t unknown = 0; // 0x00 - always 0 in source, no known delta setter
    uint8_t privFlag = 0; // 0x01 - admin/CSR level, confirmed via setAdminLevel

    // Implemented via the schema engine (see kPlayerObjectBaseline6Schema
    // below) - PLAN.md's schema-engine plan, Stage 5.
    static ParseResult<PlayerObjectBaseline6> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. No ancestor (PLAY6 is standalone).
inline constexpr FieldDescriptor kPlayerObjectBaseline6Fields[] = {
    field<FieldKind::Int32, &PlayerObjectBaseline6::unknown>(0x00, "unknown"),
    field<FieldKind::Byte, &PlayerObjectBaseline6::privFlag>(0x01, "privFlag"),
};

inline constexpr ObjectSchema kPlayerObjectBaseline6Schema{nullptr, 0,
                                                             kPlayerObjectBaseline6Fields};

} // namespace swgproto
