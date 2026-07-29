#include "worldmodel/PortalVisibility.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace worldmodel {

namespace {

struct BoundingSphere {
    assets::Float3 center{};
    float radius = 0.0f;
};

// Same real approximation this project's own segmentCrossesPortal()
// (tools/dummyclient/Visualizer.cpp) already uses for door-sized SWG
// portals: centroid of the shape's real vertices, radius = the farthest
// vertex's distance from that centroid.
BoundingSphere computePortalBoundingSphere(const assets::PortalShape& shape) {
    BoundingSphere sphere;
    if (shape.vertices.empty()) {
        return sphere;
    }
    for (const assets::Float3& v : shape.vertices) {
        sphere.center.x += v.x;
        sphere.center.y += v.y;
        sphere.center.z += v.z;
    }
    float count = static_cast<float>(shape.vertices.size());
    sphere.center.x /= count;
    sphere.center.y /= count;
    sphere.center.z /= count;

    float maxDistSq = 0.0f;
    for (const assets::Float3& v : shape.vertices) {
        float dx = v.x - sphere.center.x;
        float dy = v.y - sphere.center.y;
        float dz = v.z - sphere.center.z;
        maxDistSq = std::max(maxDistSq, dx * dx + dy * dy + dz * dz);
    }
    sphere.radius = std::sqrt(maxDistSq);
    return sphere;
}

} // namespace

std::vector<bool> computeVisibleCells(
    const std::vector<std::vector<assets::CellPortal>>& cellPortals,
    const std::vector<assets::PortalShape>& portalShapes, size_t startCellIndex,
    const std::function<bool(assets::Float3 center, float radius)>& isSphereVisible, int maxDepth) {
    std::vector<bool> visible(cellPortals.size(), false);
    if (startCellIndex >= cellPortals.size()) {
        return visible;
    }

    std::vector<int> depthOf(cellPortals.size(), -1);
    std::queue<size_t> pending;

    visible[startCellIndex] = true;
    depthOf[startCellIndex] = 0;
    pending.push(startCellIndex);

    while (!pending.empty()) {
        size_t current = pending.front();
        pending.pop();
        int currentDepth = depthOf[current];
        if (currentDepth >= maxDepth) {
            continue;
        }

        for (const assets::CellPortal& portal : cellPortals[current]) {
            if (portal.adjacentCellIndex >= cellPortals.size() || visible[portal.adjacentCellIndex]) {
                continue;
            }
            if (portal.portalShapeIndex >= portalShapes.size()) {
                continue;
            }
            BoundingSphere sphere = computePortalBoundingSphere(portalShapes[portal.portalShapeIndex]);
            if (!isSphereVisible(sphere.center, sphere.radius)) {
                continue;
            }
            visible[portal.adjacentCellIndex] = true;
            depthOf[portal.adjacentCellIndex] = currentDepth + 1;
            pending.push(portal.adjacentCellIndex);
        }
    }

    return visible;
}

} // namespace worldmodel
