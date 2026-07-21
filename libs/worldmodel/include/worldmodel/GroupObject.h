#pragma once

#include <optional>

#include "swgproto/GroupObjectBaseline3.h"
#include "swgproto/GroupObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// GroupObject (tag "GRUP") has no confirmed live delta for BASE3 (no
// GroupObjectDeltaMessage3 class exists in source at all), so only BASE6
// gets delta support - see ObjectStore.cpp's registration for why BASE6's
// delta bypasses the generic applyDelta<> template entirely
// (swgproto::applyGroupObjectBaseline6Delta, GroupObjectDelta6.h).
struct GroupObject : WorldObject {
    std::optional<swgproto::GroupObjectBaseline3> base3;
    std::optional<swgproto::GroupObjectBaseline6> base6;
};

} // namespace worldmodel
