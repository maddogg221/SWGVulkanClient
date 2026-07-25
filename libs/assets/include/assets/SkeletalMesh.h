#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/StaticMesh.h" // Float2/Float3

namespace assets {

// One bone-index/weight pair for a single mesh-level vertex - a vertex may
// be influenced by more than one bone (real per-vertex weight counts vary,
// see TWHD's own per-vertex count field).
struct BoneWeight {
    uint32_t boneIndex = 0;
    float weight = 0.0f;
};

// One real shader/material submesh. Real skeletal meshes can have more than
// one (e.g. body + a separate face/eye shader), each with its own flattened
// vertex set and local triangle index buffer - already resolved here out of
// the real format's PIDX/NIDX two-level indirection (triangle indices in a
// real PSDT block index into PIDX/NIDX, which in turn index into the
// mesh-level POSN/NORM arrays), so this is directly renderable the same way
// assets::MeshData already is for static meshes.
struct SkeletalMeshSubmesh {
    std::string shaderFilename;
    std::vector<Float3> positions;
    std::vector<Float3> normals;
    std::vector<Float2> uv0;
    std::vector<uint32_t> indices;
    // Real PIDX value each flattened `positions[i]`/`normals[i]` came from -
    // i.e. its index into the mesh-level `SkeletalMeshData::vertexWeights`
    // (added Phase 21, animation) - lets a runtime skinning pass look up
    // each flattened vertex's real bone weights without re-deriving the
    // PIDX/NIDX indirection this constructor already resolved once.
    std::vector<uint32_t> sourceVertexIndices;
};

// Plain, renderer-agnostic skinned-mesh data. Real files can legitimately
// have zero submeshes (a confirmed real anomaly, not a parse failure - see
// SkeletalMesh::parse()'s own comment); `vertexWeights` is carried through
// unconsumed this phase (bind-pose rendering only needs `submeshes`) so a
// future animation/skinning pass doesn't need to re-parse this file.
struct SkeletalMeshData {
    std::string skeletonFilename; // full archive path (e.g. "appearance/skeleton/frog.skt")
    std::vector<std::string> boneNames; // parallel to BoneWeight::boneIndex
    std::vector<std::vector<BoneWeight>> vertexWeights; // indexed by mesh-level POSN order
    std::vector<SkeletalMeshSubmesh> submeshes;
};

class SkeletalMesh {
public:
    // Parses a real ".mgn" file (FORM SKMG). Throws std::runtime_error only
    // for genuinely malformed input (missing required chunks, size
    // mismatches) - a mesh with zero submeshes is NOT an error, callers
    // must treat `submeshes.empty()` as "no renderable triangles" and fall
    // back accordingly (see the skeletal-mesh resolver's own fallback
    // requirement).
    static SkeletalMeshData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
