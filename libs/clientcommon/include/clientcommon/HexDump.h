#pragma once

#include <cstdint>
#include <vector>

namespace clientcommon {

// Prints `bytes` as space-separated, zero-padded lowercase hex to
// std::cout, no trailing newline. Shared between dummyclient's interactive
// capture mode and pcapdecoder - both need to hex-dump undecoded
// ObjControllerMessage sub-types for later analysis (see PHASE_04_STATUS.md's
// roadmap of not-yet-decoded types). Was duplicated identically in both
// tools before this - see PHASE_05_STATUS.md.
void printHexBytes(const std::vector<uint8_t>& bytes);

} // namespace clientcommon
