#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

// Reads a client ".iff" object template's `appearanceFilename` and
// `portalLayoutFilename` fields (see SharedObjectTemplate.h in the Core3
// reference clone for the full real field list, of which these are two).
// Real, confirmed-live distinction (Phase 16): simple items/creatures set
// `appearanceFilename` and leave `portalLayoutFilename` empty; real player
// structures (houses, guildhalls) do the OPPOSITE - `appearanceFilename` is
// a genuinely present-but-empty chunk, and `portalLayoutFilename` (a real
// ".pob" FORM PRTO portal/cell layout file - see BuildingLayout.h) is what
// actually carries their geometry. Both fields simply default empty if
// their chunk isn't found - callers branch on which one is non-empty.
struct SharedObjectTemplateData {
    std::string appearanceFilename;
    std::string portalLayoutFilename;
};

class SharedObjectTemplate {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected, or
    // if NEITHER appearanceFilename nor portalLayoutFilename is found (every
    // real template checked so far has at least one).
    static SharedObjectTemplateData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
