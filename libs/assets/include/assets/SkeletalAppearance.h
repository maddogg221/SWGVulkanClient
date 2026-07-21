#pragma once

#include <string>
#include <vector>

namespace assets {

// Plain data from a real ".sat" file (FORM SMAT) - the entry point a real
// creature/player template's appearance chain resolves to, tying together
// its body-part meshes and skeleton. Real structure confirmed by dumping an
// actual extracted .sat: FORM(SMAT)/FORM(0003) -> CHNK INFO (numMSGN uint32
// + numSKTI uint16) -> CHNK MSGN (numMSGN NUL-terminated ".lmg" body-part
// filenames - full archive paths, e.g. "appearance/mesh/gubbur.lmg", same
// as SkeletalMeshLod.h's own NAME chunks) -> CHNK SKTI (numSKTI pairs of
// [skeletonFile, name] strings - only the skeleton filename is read; the
// second string was empty in the one real file checked this session, its
// purpose unclear and not needed) -> CHNK LATX (numSKTI pairs of
// [skeletonFile, ".lat" animation-state-table filename] - real, confirmed
// to exist, not consumed this phase since animation is out of scope).
struct SkeletalAppearanceData {
    std::vector<std::string> meshFilenames; // full archive paths, each a ".lmg"
    std::string skeletonFilename;           // full archive path, a ".skt"
};

class SkeletalAppearance {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected or
    // no skeleton reference is found.
    static SkeletalAppearanceData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
