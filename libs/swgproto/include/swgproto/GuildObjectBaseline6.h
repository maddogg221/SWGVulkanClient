#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// GuildObject's BASE6 baseline (tag "GILD"). Standalone, no ancestor.
// Trivial: GuildObjectMessage6.h's constructor writes a single literal
// int32 (0x3B = 59) and nothing else. No GuildObjectDeltaMessage6.h exists
// in source at all, so nothing here is delta-addressable. Verified live
// against a real 4-byte capture matching this exactly.
struct GuildObjectBaseline6 {
    int32_t unknownConst = 0; // always 0x3B (59)

    static ParseResult<GuildObjectBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kGuildObjectBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &GuildObjectBaseline6::unknownConst>("unknownConst"),
};

inline constexpr ObjectSchema kGuildObjectBaseline6Schema{nullptr, 0, kGuildObjectBaseline6Fields};

} // namespace swgproto
