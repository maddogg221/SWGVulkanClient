#include "assets/DdsImage.h"

#include <cstring>
#include <stdexcept>

namespace assets {

namespace {

// The standard Microsoft DDS layout (public/documented, not reverse-
// engineered - see DDS_HEADER/DDS_PIXELFORMAT in the public DDS spec).
// Real byte offsets, confirmed against several real client textures via a
// direct header dump:
//   0   : magic, 4 bytes, ASCII "DDS "
//   4   : DDS_HEADER starts (124 bytes total)
//     8   (buffer offset 12): dwHeight
//     12  (buffer offset 16): dwWidth
//     16  (buffer offset 20): dwPitchOrLinearSize - not read, recomputed instead
//     24  (buffer offset 28): dwMipMapCount - not read, only the base mip matters here
//     72  (buffer offset 76): DDS_PIXELFORMAT starts (32 bytes)
//       76  (buffer offset 80): ddspf.dwFlags
//       80  (buffer offset 84): ddspf.dwFourCC - "DXT1"/"DXT3"/"DXT5" for
//                                every real texture confirmed so far
//   128 : pixel/block data begins (base mip level first)
constexpr size_t kHeaderTotalSize = 128;
constexpr size_t kHeightOffset = 12;
constexpr size_t kWidthOffset = 16;
constexpr size_t kFourCcOffset = 84;

uint32_t readU32At(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value; // DDS fields are little-endian, matching this machine's own byte order
}

} // namespace

DdsImageData DdsImage::parse(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kHeaderTotalSize) {
        throw std::runtime_error("DdsImage::parse: buffer smaller than the fixed DDS header size");
    }
    if (std::memcmp(bytes.data(), "DDS ", 4) != 0) {
        throw std::runtime_error("DdsImage::parse: missing 'DDS ' magic");
    }

    DdsImageData result;
    result.height = readU32At(bytes, kHeightOffset);
    result.width = readU32At(bytes, kWidthOffset);

    uint32_t fourCC = readU32At(bytes, kFourCcOffset);
    uint32_t bytesPerBlock = 0;
    if (fourCC == 0x31545844) { // "DXT1"
        result.format = DdsBlockFormat::Bc1;
        bytesPerBlock = 8;
    } else if (fourCC == 0x33545844) { // "DXT3"
        result.format = DdsBlockFormat::Bc2;
        bytesPerBlock = 16;
    } else if (fourCC == 0x35545844) { // "DXT5"
        result.format = DdsBlockFormat::Bc3;
        bytesPerBlock = 16;
    } else {
        throw std::runtime_error("DdsImage::parse: unsupported/non-FourCC pixel format");
    }

    if (result.width == 0 || result.height == 0) {
        throw std::runtime_error("DdsImage::parse: zero width/height");
    }

    // Block-compressed formats store 4x4 pixel blocks - a dimension not
    // evenly divisible by 4 still occupies one full block (real DDS
    // convention, matches every reference decoder).
    uint32_t blocksWide = (result.width + 3) / 4;
    uint32_t blocksHigh = (result.height + 3) / 4;
    size_t baseMipSize = static_cast<size_t>(blocksWide) * blocksHigh * bytesPerBlock;

    if (bytes.size() < kHeaderTotalSize + baseMipSize) {
        throw std::runtime_error("DdsImage::parse: buffer too small for the base mip level");
    }

    result.blockData.assign(bytes.begin() + kHeaderTotalSize,
                             bytes.begin() + kHeaderTotalSize + baseMipSize);
    return result;
}

} // namespace assets
