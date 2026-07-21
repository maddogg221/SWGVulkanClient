#pragma once

#include <optional>

#include "swgproto/HarvesterObjectBaseline7.h"
#include "swgproto/InstallationObjectBaseline3.h"
#include "swgproto/TangibleObjectBaseline6.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// BASE6 reuses TangibleObjectBaseline6's struct/schema unchanged - no
// dedicated InstallationObjectBaseline6 exists in swgproto (Core3's
// InstallationObjectMessage6 constructor adds zero fields beyond
// TangibleObjectMessage6, same dead-constructor-body pattern as
// WeaponObjectMessage6) - so this slot's type is literally
// swgproto::TangibleObjectBaseline6, same as TangibleObject's own base6.
//
// `base7` is HarvesterObject-specific (tag "HINO", not "INSO") - composed
// onto this SAME type rather than a separate HarvesterObject concrete type,
// since a real harvester's BASE3/BASE6 arrive under the generic "INSO" tag
// (HarvesterObjectMessage3/6 are confirmed dead code) while only BASE7 uses
// "HINO" - a separate concrete type would collide with this one in
// ObjectStore's variant for the same objectId. Present only for objects
// that are actually harvesters; absent (std::nullopt) for every other
// InstallationObject.
struct InstallationObject : WorldObject {
    std::optional<swgproto::InstallationObjectBaseline3> base3;
    std::optional<swgproto::TangibleObjectBaseline6> base6;
    std::optional<swgproto::HarvesterObjectBaseline7> base7;
};

} // namespace worldmodel
