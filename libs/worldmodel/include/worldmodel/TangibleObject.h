#pragma once

#include <optional>

#include "swgproto/TangibleObjectBaseline3.h"
#include "swgproto/TangibleObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// One optional slot per baseline number swgproto defines a schema for - the
// mechanical composition rule this whole library follows (see PLAN.md's
// Object Model design). The struct type is always swgproto's own, verbatim -
// never re-declared or flattened. Whether that struct composes another
// baseline internally (it doesn't here - TangibleObject is the root of the
// composition chain) is entirely swgproto's concern.
struct TangibleObject : WorldObject {
    std::optional<swgproto::TangibleObjectBaseline3> base3;
    std::optional<swgproto::TangibleObjectBaseline6> base6;
};

} // namespace worldmodel
