#pragma once

#include <optional>

#include "swgproto/CreatureObjectBaseline1.h"
#include "swgproto/CreatureObjectBaseline3.h"
#include "swgproto/CreatureObjectBaseline4.h"
#include "swgproto/CreatureObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// One optional slot per baseline number swgproto defines a schema for.
// BASE1/BASE4 are standalone; BASE3/BASE6 each compose TangibleObjectBaseline3/6
// internally (via their own `.tangible` member) - that composition is entirely
// swgproto's concern, inherited unchanged here, per this library's mechanical
// composition rule (see PLAN.md's Object Model design).
struct CreatureObject : WorldObject {
    std::optional<swgproto::CreatureObjectBaseline1> base1;
    std::optional<swgproto::CreatureObjectBaseline3> base3;
    std::optional<swgproto::CreatureObjectBaseline4> base4;
    std::optional<swgproto::CreatureObjectBaseline6> base6;
};

} // namespace worldmodel
