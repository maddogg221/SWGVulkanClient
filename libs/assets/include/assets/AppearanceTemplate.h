#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

// Reads a client ".apt" appearance template. Real structure (confirmed by
// dumping an actual extracted .apt): FORM(APT )/FORM(0000)/NAME - a single
// NUL-terminated string. For every real static object template checked
// this session, that string points to a ".lod" file (e.g.
// "appearance/lod/thm_prp_crate_spice.lod"), NOT a .msh directly - the
// actual mesh reference is one hop further inside the LOD file (see
// LodFile.h). Kept as a generic "next reference" rather than assumed to
// always be a .lod, in case some templates point straight at a .msh.
struct AppearanceTemplateData {
    std::string referencedFilename;
};

class AppearanceTemplate {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected or
    // no NAME chunk is found.
    static AppearanceTemplateData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
