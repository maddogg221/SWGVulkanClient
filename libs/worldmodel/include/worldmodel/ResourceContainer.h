#pragma once

#include <optional>

#include "swgproto/ResourceContainerBaseline3.h"
#include "swgproto/ResourceContainerBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// ResourceContainer is the clean proof of this library's composition rule:
// BASE3's swgproto schema composes TangibleObjectBaseline3 (ancestor !=
// nullptr, embedded as ResourceContainerBaseline3::tangible), while BASE6's
// schema is standalone (ancestor == nullptr) - same real-world object type,
// different wire composition per baseline number. Both live here as two
// independent optional slots, exactly reflecting that variance - this
// library never attempts to unify them into one shared shape.
struct ResourceContainer : WorldObject {
    std::optional<swgproto::ResourceContainerBaseline3> base3;
    std::optional<swgproto::ResourceContainerBaseline6> base6;
};

} // namespace worldmodel
