#pragma once

#include <optional>

#include "swgproto/IntangibleObjectBaseline3.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// Only BASE3 is decoded in swgproto (BASE6's opcnt/constructor field count
// disagree in source - a separate, still-open task, see PLAN.md's known
// gaps) - so this type has only one slot so far, unlike most others.
struct IntangibleObject : WorldObject {
    std::optional<swgproto::IntangibleObjectBaseline3> base3;
};

} // namespace worldmodel
