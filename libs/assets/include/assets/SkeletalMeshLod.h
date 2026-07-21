#pragma once

#include <string>
#include <vector>

namespace assets {

// Reads a client ".lmg" (skeletal mesh level-of-detail group) file - what a
// real .sat/SMAT's MSGN chunk actually points to (see SkeletalAppearance.h).
// Real structure confirmed by dumping an actual extracted .lmg:
// FORM(MLOD)/FORM(0000) contains one 'NAME' leaf chunk per LOD level (highest
// detail first, in the one real file checked this session - not assumed
// stable, see parse()'s own selection logic), each a NUL-terminated ".mgn"
// path. Unlike ".lod"'s CHLD entries (relative to "appearance/" - see
// LodFile.h), these are already FULL archive paths (e.g.
// "appearance/mesh/gubbur_l0.mgn") - confirmed against real bytes, no
// prefix needed by callers. Structurally distinct from the
// unrelated-but-analogous ".lod" format LodFile.h already parses (no
// distance-float prefix, no 'DTLA' outer tag), so it needs its own small
// parser rather than reusing LodFile directly.
struct SkeletalMeshLodData {
    std::string highestDetailMeshFilename; // full archive path
};

class SkeletalMeshLod {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected or
    // no NAME chunk is found. Picks the mesh path whose filename ends in
    // "_l0.mgn" if present (the real, explicit "highest detail" marker this
    // project has seen); falls back to the first NAME chunk otherwise,
    // matching LodFile::parse()'s own fallback precedent.
    static SkeletalMeshLodData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
