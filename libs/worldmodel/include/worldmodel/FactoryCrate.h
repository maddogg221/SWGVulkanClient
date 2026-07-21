#pragma once

#include <optional>

#include "swgproto/FactoryCrateBaseline3.h"
#include "swgproto/FactoryCrateBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// A stack of identical crafted items. Both baselines standalone (despite
// BASE3 having a Tangible-shaped field list, it does NOT extend
// TangibleObjectMessage3 in source - coincidental shape, not real
// composition, per this library's mechanical composition rule). BASE6
// carries zero live data (all hardcoded constants in source) - no delta
// message class exists for it at all.
struct FactoryCrate : WorldObject {
    std::optional<swgproto::FactoryCrateBaseline3> base3;
    std::optional<swgproto::FactoryCrateBaseline6> base6;
};

} // namespace worldmodel
