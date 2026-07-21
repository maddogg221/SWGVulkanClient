#pragma once

#include <cstdint>
#include <vector>

namespace assets {

struct Rgb8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

// A single-row (height == 1), uncompressed 24-bit TGA image - the real
// shape every terrain color-ramp image referenced by a real .trn file's
// AffectorColorRampHeight/AffectorColorRampFractal has been confirmed to
// use (checked against naboo.trn's own 28 distinct ramp images, all
// TGA image type 2, colorMapType 0, bitsPerPixel 24, height 1; sizes
// verified byte-for-byte against the standard 18-byte header + pixel data
// + optional 26-byte TGA 2.0 footer). This is deliberately NOT a general
// TGA reader - anything outside that shape (compressed, color-mapped,
// non-24bpp, more than one row) throws rather than guessing, since no
// real terrain ramp image has ever been observed to need it.
struct TgaImage {
    int width = 0;
    std::vector<Rgb8> pixels; // row-major, `width` entries (height is always 1)

    // Throws std::runtime_error for anything outside the confirmed real
    // shape described above.
    static TgaImage parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
