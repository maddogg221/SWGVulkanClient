#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal, dependency-light PNG writer for diagnostic screenshot capture
// (see VulkanRenderer::captureFrameRGBA8's own comment on why this exists -
// walk-animation debugging needed real comparable frame images, not just
// numbers). Only ever needs to write what this project's own renderer
// produces: tightly packed, top-to-bottom RGBA8. Uses zlib (already a
// project dependency via swg::zlib, transitively linked through
// renderer->assets) for the required DEFLATE compression and CRC32 - no new
// third-party dependency. Not a general-purpose PNG encoder (always 8-bit
// RGBA, always filter type "None", no interlacing, no palette) - deliberately
// scoped to this one real use case.
namespace dummyclient {

// Returns false (and writes nothing) on any I/O or compression failure.
bool writePngRGBA8(const std::string& path, uint32_t width, uint32_t height,
                    const std::vector<uint8_t>& rgba);

} // namespace dummyclient
