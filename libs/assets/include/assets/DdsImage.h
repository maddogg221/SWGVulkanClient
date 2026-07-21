#pragma once

#include <cstdint>
#include <vector>

namespace assets {

// Which real GPU block-compression format a DdsImageData's `blockData`
// holds. Real client textures confirmed via a direct header dump of several
// real files (crate/interior/terminal textures) to use only these two - a
// public, fully-documented Microsoft format (unlike every proprietary SWG
// format this project has had to reverse-engineer), so the risk here was
// "which real variant(s) actually appear," not format discovery.
enum class DdsBlockFormat {
    Bc1, // 'DXT1' - 8 bytes/4x4 block, opaque or 1-bit alpha
    Bc2, // 'DXT3' - 16 bytes/4x4 block, explicit 4-bit alpha (not yet confirmed in real data, supported defensively)
    Bc3, // 'DXT5' - 16 bytes/4x4 block, interpolated alpha
};

// Plain, renderer-agnostic compressed-texture data - deliberately just the
// TOP (largest) mip level's raw compressed block bytes, not the full mip
// chain (real files carry ~9 real mip levels; generating/uploading the rest
// is deferred, matching this pass' "prove it looks right first" scope - see
// the project plan's own note). No DirectX/D3D types, same "zero rendering
// dependency" convention as assets::MeshData.
struct DdsImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    DdsBlockFormat format = DdsBlockFormat::Bc1;
    std::vector<uint8_t> blockData; // raw compressed bytes for the base mip level only
};

class DdsImage {
public:
    // Parses a real ".dds" file (the standard 4-byte "DDS " magic + 124-byte
    // DDS_HEADER layout - see DdsImage.cpp's own comment for the exact real
    // field offsets used). Throws std::runtime_error for anything other
    // than an FourCC-compressed DXT1/DXT3/DXT5 file (the only real variants
    // confirmed in this project's own client archives) - an uncompressed or
    // DX10-extension-header file would need real bytes to confirm before
    // adding support, not guessed at.
    static DdsImageData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
