#pragma once

#include <optional>

#include "swgproto/CellObjectBaseline3.h"
#include "swgproto/CellObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// One instance per room in a building interior, not a free-roaming entity.
// Both baselines standalone (extend BaseLineMessage directly, no
// TangibleObject ancestor). No delta message class exists for BASE6 in
// source at all - only BASE3's cellNumber (index 0x05) is delta-addressable.
struct CellObject : WorldObject {
    std::optional<swgproto::CellObjectBaseline3> base3;
    std::optional<swgproto::CellObjectBaseline6> base6;
};

} // namespace worldmodel
