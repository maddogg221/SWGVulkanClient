#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/StaticMesh.h" // Float3

namespace assets {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct SkeletonBone {
    std::string name;
    int32_t parentIndex = -1; // -1 = root
    Quaternion preRotation;
    Quaternion postRotation;
    Float3 bindTranslation;
};

// Plain, renderer-agnostic bind-pose skeleton data. Deliberately excludes
// `BPRO`/`JROR` (real chunks present in every `.skt` file, but whose
// semantics haven't been needed/decoded yet - see Skeleton.cpp's own
// comment) since bind-pose rendering only needs per-bone parent index +
// translation + orientation.
struct SkeletonData {
    std::vector<SkeletonBone> bones;
};

class Skeleton {
public:
    // Parses a real ".skt" file. Real files are `FORM SLOD` - a
    // level-of-detail wrapper containing multiple nested `FORM SKTM` blocks
    // at decreasing bone count (one per skeleton LOD level) - not a single
    // skeleton directly. This picks the block with the most bones (the
    // highest-detail one). Throws std::runtime_error if the buffer doesn't
    // parse as expected.
    static SkeletonData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
