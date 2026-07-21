#pragma once

#include <optional>

#include "swgproto/TangibleObjectBaseline3.h"
#include "swgproto/TangibleObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// WeaponObject's wire format is byte-identical to TangibleObject today (Core3
// reuses TangibleObjectMessage3/6's exact schema, zero new fields exist in
// source) - but it gets its own distinct Object Model type anyway, a
// deliberate decision resolved via a direct question during this library's
// design pass: room for weapon-specific fields (damage, speed) future
// combat/gameplay code will want, matching the goal of typed wrappers
// providing real semantic clarity, not just a wire-format mirror.
struct WeaponObject : WorldObject {
    std::optional<swgproto::TangibleObjectBaseline3> base3;
    std::optional<swgproto::TangibleObjectBaseline6> base6;
};

} // namespace worldmodel
