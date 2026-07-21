#pragma once

#include <optional>

#include "swgproto/PlayerObjectBaseline3.h"
#include "swgproto/PlayerObjectBaseline6.h"
#include "swgproto/PlayerObjectBaseline8.h"
#include "swgproto/PlayerObjectBaseline9.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// PLAY baselines are confirmed self-only from source (only ever sent for the
// connection's own attached "ghost" PlayerObject, a separate object from the
// CreatureObject itself, linked via containment) - but that objectId is NOT
// the character's own objectId, so WorldObject::isSelf will correctly read
// false for this object (it genuinely isn't "self", it's self's ghost).
// ObjectStore does not attempt self-ghost identification (needs
// UpdateContainmentMessage consumption, deliberately out of scope per
// PLAN.md's Object Model design) - it just stores whatever objectId PLAY
// traffic arrives under, which in practice is always the one real ghost
// object, since nothing else is ever sent under this tag.
struct PlayerObject : WorldObject {
    std::optional<swgproto::PlayerObjectBaseline3> base3;
    std::optional<swgproto::PlayerObjectBaseline6> base6;
    std::optional<swgproto::PlayerObjectBaseline8> base8;
    std::optional<swgproto::PlayerObjectBaseline9> base9;
};

} // namespace worldmodel
