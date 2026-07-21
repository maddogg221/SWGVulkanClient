#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

// Reads a client ".lod" (level-of-detail group) file - what a real .apt
// appearance template actually points to (see AppearanceTemplate.h's
// comment). Real structure confirmed by dumping an actual extracted .lod:
// FORM(DTLA)/FORM(0007)/FORM(DATA) contains one 'CHLD' leaf chunk per LOD
// level (highest detail last, in the one real file checked this session -
// not assumed stable, see parse()'s own selection logic), each holding a
// 4-byte float (distance threshold - not read by this project) followed by
// a NUL-terminated mesh path RELATIVE to "appearance/" (e.g.
// "mesh/thm_prp_crate_spice_l0.msh" - resolvers must prepend "appearance/"
// themselves, matching how the real archives are laid out).
struct LodFileData {
    std::string highestDetailMeshFilename; // relative to "appearance/"
};

class LodFile {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected or
    // no CHLD chunk is found. Picks the mesh path whose filename ends in
    // "_l0.msh" if present (the real, explicit "highest detail" marker this
    // project has seen); falls back to the first CHLD chunk otherwise
    // rather than failing outright, since not every object necessarily
    // follows the "_l0/_l1/_l2" naming convention.
    static LodFileData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
