#include "terrain/Boundary.h"

#include <stdexcept>

#include "GeneratorItem.h"
#include "Tags.h"

namespace terrain {

namespace {

Boundary parseBoundaryCircle(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(2), "BoundaryCircle");
    BoundaryCircle shape;
    shape.centerX = v.data.readFloat();
    shape.centerZ = v.data.readFloat();
    shape.radius = v.data.readFloat();
    shape.featherFunction = static_cast<int>(v.data.readUint32());
    shape.featherDistance = readClampedFeatherDistance(v.data);
    return Boundary{v.header, shape};
}

Boundary parseBoundaryRectangle(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(2), "BoundaryRectangle");
    BoundaryRectangle shape;
    shape.x0 = v.data.readFloat();
    shape.y0 = v.data.readFloat();
    shape.x1 = v.data.readFloat();
    shape.y1 = v.data.readFloat();
    shape.featherFunction = static_cast<int>(v.data.readUint32());
    shape.featherDistance = readClampedFeatherDistance(v.data);
    return Boundary{v.header, shape};
}

Boundary parseBoundaryPolygon(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(5), "BoundaryPolygon");
    BoundaryPolygon shape;
    shape.points = readPointList2D(v.data);
    shape.featherFunction = static_cast<int>(v.data.readUint32());
    shape.featherDistance = readClampedFeatherDistance(v.data);
    shape.localWaterTable = v.data.readUint32() != 0;
    shape.localWaterTableHeight = v.data.readFloat();
    shape.localWaterTableShaderSize = v.data.readFloat();
    shape.localWaterTableShaderTemplateName = assets::readNulTerminatedString(v.data);
    return Boundary{v.header, shape};
}

Boundary parseBoundaryPolyline(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(1), "BoundaryPolyline");
    BoundaryPolyline shape;
    shape.points = readPointList2D(v.data);
    shape.featherFunction = static_cast<int>(v.data.readUint32());
    shape.featherDistance = readClampedFeatherDistance(v.data);
    shape.width = v.data.readFloat();
    return Boundary{v.header, shape};
}

} // namespace

bool isBoundaryTag(uint32_t tag) {
    return tag == kTagBALL || tag == kTagBCIR || tag == kTagBREC || tag == kTagBPOL ||
           tag == kTagBSPL || tag == kTagBPLN;
}

std::optional<Boundary> parseBoundary(const assets::IffChunk& form) {
    switch (form.formType) {
        case kTagBALL:
        case kTagBSPL:
            // Dead tags - the real engine enters then unconditionally skips
            // these, never constructing an object.
            return std::nullopt;
        case kTagBCIR:
            return parseBoundaryCircle(form);
        case kTagBREC:
            return parseBoundaryRectangle(form);
        case kTagBPOL:
            return parseBoundaryPolygon(form);
        case kTagBPLN:
            return parseBoundaryPolyline(form);
        default:
            throw std::runtime_error("terrain: parseBoundary called with a non-boundary tag");
    }
}

} // namespace terrain
