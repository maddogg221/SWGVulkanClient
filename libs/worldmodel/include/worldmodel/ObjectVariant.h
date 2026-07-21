#pragma once

#include <variant>

#include "worldmodel/CellObject.h"
#include "worldmodel/CreatureObject.h"
#include "worldmodel/FactoryCrate.h"
#include "worldmodel/GroupObject.h"
#include "worldmodel/InstallationObject.h"
#include "worldmodel/IntangibleObject.h"
#include "worldmodel/PlayerObject.h"
#include "worldmodel/ResourceContainer.h"
#include "worldmodel/StaticObject.h"
#include "worldmodel/TangibleObject.h"
#include "worldmodel/WeaponObject.h"

namespace worldmodel {

// Top-level ownership: a tagged sum type rather than a virtual base +
// dynamic_cast (see WorldObject.h's design note) or per-type maps + a
// separate id->tag index. std::variant makes "which concrete type is this"
// and "what's its data" the same value - there is no way for a tag and its
// payload to disagree, unlike a hand-maintained parallel index would allow.
//
// Every object type this project has decoded now has a slot here (`GuildObject`
// is the one confirmed exception - see SingletonRegistry.h for why it's
// structurally separate). `HarvesterObject`/`GroupObject` (researched, not
// yet implemented - see PLAN.md's Phase 6 Tier 4) will add their own
// alternatives once they're actually wired into ObjectStore, not before.
using ObjectVariant =
    std::variant<TangibleObject, ResourceContainer, CreatureObject, PlayerObject,
                 WeaponObject, IntangibleObject, InstallationObject, CellObject,
                 FactoryCrate, StaticObject, GroupObject>;

} // namespace worldmodel
