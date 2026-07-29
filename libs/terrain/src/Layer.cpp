#include "terrain/Layer.h"

#include <stdexcept>
#include <variant>

#include "Tags.h"

namespace terrain {

Layer parseLayer(const assets::IffChunk& layrForm) {
    if (layrForm.formType != kTagLAYR) {
        throw std::runtime_error("terrain: parseLayer called with a non-LAYR form");
    }
    if (layrForm.children.empty() || layrForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: Layer missing version FORM");
    }
    const assets::IffChunk& versionForm = layrForm.children[0];
    if (versionForm.formType != versionTag(3)) {
        throw std::runtime_error("terrain: Layer unsupported version (real files only use 0003)");
    }
    if (versionForm.children.empty() || versionForm.children[0].id != assets::kFormTag ||
        versionForm.children[0].formType != kTagIHDR) {
        throw std::runtime_error("terrain: Layer missing IHDR");
    }

    Layer layer;
    layer.header = parseLayerItemHeader(versionForm.children[0]);

    const assets::IffChunk* adta = versionForm.findChild(kTagADTA);
    if (adta == nullptr) {
        throw std::runtime_error("terrain: Layer missing CHNK ADTA");
    }
    soe::PacketBuffer buf = adta->data;
    buf.resetReadCursor();
    layer.invertBoundaries = buf.readUint32() != 0;
    layer.invertFilters = buf.readUint32() != 0;
    layer.expanded = buf.readUint32() != 0;
    layer.notes = assets::readNulTerminatedString(buf);

    // Remaining siblings (everything after IHDR, skipping ADTA itself) are
    // dispatched purely by FORM tag - boundary, then filter, then affector,
    // then it must be a nested FORM LAYR.
    for (size_t i = 1; i < versionForm.children.size(); ++i) {
        const assets::IffChunk& child = versionForm.children[i];
        if (child.id == kTagADTA) {
            continue;
        }
        if (child.id != assets::kFormTag) {
            throw std::runtime_error("terrain: Layer: unexpected non-FORM child");
        }
        uint32_t tag = child.formType;
        if (isBoundaryTag(tag)) {
            std::optional<Boundary> boundary = parseBoundary(child);
            if (boundary.has_value()) {
                layer.boundaries.push_back(std::move(*boundary));
            }
        } else if (isFilterTag(tag)) {
            layer.filters.push_back(parseFilter(child));
        } else if (isAffectorTag(tag)) {
            std::optional<Affector> affector = parseAffector(child);
            if (affector.has_value()) {
                layer.affectors.push_back(std::move(*affector));
            }
        } else if (tag == kTagLAYR) {
            layer.subLayers.push_back(parseLayer(child));
        } else {
            throw std::runtime_error("terrain: Layer: unrecognized child tag");
        }
    }

    return layer;
}

void translateLayerBoundaries(Layer& layer, float dx, float dz) {
    for (Boundary& boundary : layer.boundaries) {
        std::visit(
            [&](auto& shape) {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, BoundaryCircle>) {
                    shape.centerX += dx;
                    shape.centerZ += dz;
                } else if constexpr (std::is_same_v<T, BoundaryRectangle>) {
                    shape.x0 += dx;
                    shape.x1 += dx;
                    shape.y0 += dz;
                    shape.y1 += dz;
                } else { // BoundaryPolygon, BoundaryPolyline - both { points: vector<Float2> }
                    for (assets::Float2& point : shape.points) {
                        point.x += dx;
                        point.y += dz;
                    }
                }
            },
            boundary.shape);
    }
    for (Layer& subLayer : layer.subLayers) {
        translateLayerBoundaries(subLayer, dx, dz);
    }
}

void bakeLayerHeight(Layer& layer, float height) {
    constexpr int kTgoReplace = 0;
    for (Affector& affector : layer.affectors) {
        AffectorHeightConstant* heightConstant = std::get_if<AffectorHeightConstant>(&affector.affector);
        if (heightConstant != nullptr && heightConstant->operation == kTgoReplace &&
            heightConstant->height == 0.0f) {
            heightConstant->height = height;
        }
    }
    for (Layer& subLayer : layer.subLayers) {
        bakeLayerHeight(subLayer, height);
    }
}

} // namespace terrain
