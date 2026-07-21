#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

struct Float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Float2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Plain, renderer-agnostic geometry data - deliberately just arrays, no
// DirectX/D3D types (this library has zero rendering dependency; the
// renderer layer converts these into its own vertex buffers). Matches this
// pass' explicit scope: static geometry only (no bone weights), one UV set,
// no vertex colors, no collision/hardpoint/shadow-mesh data.
struct MeshData {
    std::vector<Float3> positions;
    std::vector<Float3> normals;
    std::vector<Float2> uv0;
    std::vector<uint32_t> indices;
};

// One real shader/material submesh - a real static mesh's `FORM SPS`
// (Shader Primitive Set) wraps a single version FORM whose own children are
// one FORM per real shader group actually used by the mesh (confirmed live:
// a real elevator platform has 5 - floor marble, two medical lights, a roof
// trim, and a wall fallback - each with its own real geometry). Mirrors
// SkeletalMeshSubmesh's role for skinned meshes (see SkeletalMesh.h).
struct StaticMeshSubmesh {
    std::string shaderFilename; // full archive path, e.g. "shader/intr_assoc_marblered_ces17.sht"
    MeshData mesh;
};

class StaticMesh {
public:
    // Parses a ".msh" file (versions 0004/0005 only), returning every real
    // shader-group submesh it contains - almost always more than one.
    // Throws std::runtime_error for any other version or a malformed
    // buffer. A mesh with zero submeshes is not expected but not treated
    // specially either - callers see an empty vector.
    static std::vector<StaticMeshSubmesh> parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
