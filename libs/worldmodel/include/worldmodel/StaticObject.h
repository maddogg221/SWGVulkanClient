#pragma once

#include <optional>

#include "swgproto/StaticObjectBaseline3.h"
#include "swgproto/StaticObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// World decor (campfires, benches, signs). Both baselines standalone -
// StaticObject genuinely doesn't extend TangibleObject at all in Core3's
// class hierarchy (`extends SceneObject` directly). No delta message class
// exists for either baseline number at all - every field is either set once
// at construction or a hardcoded literal, so there is zero live delta path
// here, ever.
struct StaticObject : WorldObject {
    std::optional<swgproto::StaticObjectBaseline3> base3;
    std::optional<swgproto::StaticObjectBaseline6> base6;
};

} // namespace worldmodel
