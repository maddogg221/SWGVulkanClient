#include "PngWriter.h"

#include <zlib.h>

#include <cstdio>
#include <cstring>

namespace dummyclient {

namespace {

void appendBigEndianU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// A PNG chunk is: length(4, BE) + type(4 ASCII bytes) + data + crc32(4, BE)
// over (type+data) - written as one self-contained helper since every real
// chunk this writer emits (IHDR/IDAT/IEND) follows the identical shape.
void appendChunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    appendBigEndianU32(out, static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> typeAndData(type, type + 4);
    typeAndData.insert(typeAndData.end(), data.begin(), data.end());
    out.insert(out.end(), typeAndData.begin(), typeAndData.end());
    uint32_t crc = static_cast<uint32_t>(
        crc32(0L, typeAndData.data(), static_cast<uInt>(typeAndData.size())));
    appendBigEndianU32(out, crc);
}

} // namespace

bool writePngRGBA8(const std::string& path, uint32_t width, uint32_t height,
                    const std::vector<uint8_t>& rgba) {
    if (rgba.size() != static_cast<size_t>(width) * height * 4) {
        return false;
    }

    // Real PNG raw scanline data - each row prefixed with a filter-type byte
    // (0 = "None", the simplest valid choice; this writer doesn't bother
    // with the fancier filters real encoders use to improve compression -
    // these are one-off diagnostic screenshots, not a production asset
    // pipeline).
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (1 + static_cast<size_t>(width) * 4));
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = rgba.data() + static_cast<size_t>(y) * width * 4;
        raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 4);
    }

    uLongf compressedBound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> compressed(compressedBound);
    // compress2() produces a real zlib stream (header + DEFLATE data +
    // Adler32 trailer) - exactly what a PNG IDAT chunk's own payload is
    // defined to be, no extra wrapping needed.
    if (compress2(compressed.data(), &compressedBound, raw.data(), static_cast<uLong>(raw.size()),
                  Z_DEFAULT_COMPRESSION) != Z_OK) {
        return false;
    }
    compressed.resize(compressedBound);

    std::vector<uint8_t> file;
    const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    file.insert(file.end(), signature, signature + 8);

    std::vector<uint8_t> ihdr;
    appendBigEndianU32(ihdr, width);
    appendBigEndianU32(ihdr, height);
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(6);  // color type 6 = truecolor with alpha (RGBA)
    ihdr.push_back(0);  // compression method (always 0)
    ihdr.push_back(0);  // filter method (always 0)
    ihdr.push_back(0);  // interlace method (0 = none)
    appendChunk(file, "IHDR", ihdr);

    appendChunk(file, "IDAT", compressed);
    appendChunk(file, "IEND", {});

    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr) {
        return false;
    }
    size_t written = std::fwrite(file.data(), 1, file.size(), f);
    std::fclose(f);
    return written == file.size();
}

} // namespace dummyclient
