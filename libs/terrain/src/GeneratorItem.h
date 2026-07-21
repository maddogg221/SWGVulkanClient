#pragma once

// Private (not under include/terrain) - shared parse helper for the
// "versioned FORM + IHDR + payload" shape every Boundary/Filter/Affector
// uses:
//   FORM <TAG>
//     FORM <version>
//       FORM IHDR ...          (always the first child)
//       CHNK DATA               -- most types
//       | FORM DATA / CHNK PARM -- FilterFractal/FilterBitmap and several
//                                   Affectors (AHFR/ACRF/ARIB/ARIV/AROA)
// Promoted here once a third type (Affector, after Boundary and Filter)
// needed the identical dispatch, matching this project's own "promote on
// repeat need" convention (see assets::findFirstForm's history).

#include <stdexcept>
#include <string>
#include <vector>

#include "assets/IffReader.h"
#include "assets/StaticMesh.h"
#include "terrain/LayerItem.h"
#include "Tags.h"

namespace terrain {

struct VersionedPayload {
    LayerItemHeader header;
    soe::PacketBuffer data;
};

// `expectedVersion` is the exact version real classic-planet files were
// confirmed to use for this type - any other version throws rather than
// guessing at an unverified layout. Payload is a flat sibling `CHNK DATA`.
VersionedPayload enterVersioned(const assets::IffChunk& form, uint32_t expectedVersion,
                                 const char* typeName);

// Same as enterVersioned, but the payload is nested one level deeper:
// sibling `FORM DATA` containing a `CHNK PARM`.
VersionedPayload enterVersionedNestedParm(const assets::IffChunk& form, uint32_t expectedVersion,
                                           const char* typeName);

float readClampedFeatherDistance(soe::PacketBuffer& buf);

// Shared "uint32 count, then that many x/y float pairs" shape used by
// BoundaryPolygon/BoundaryPolyline and AffectorRoad/AffectorRiver alike.
std::vector<assets::Float2> readPointList2D(soe::PacketBuffer& buf);

// Direct-child FORM search by formType - NOT findChild() (which matches by
// `id`, always kFormTag for any FORM regardless of its formType) and NOT
// assets::findFirstForm() (which also recurses into descendants; this only
// ever wants an immediate sibling).
const assets::IffChunk* findChildForm(const assets::IffChunk& parent, uint32_t formTypeTag);

} // namespace terrain
