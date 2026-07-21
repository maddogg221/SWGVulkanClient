#include "assets/TgaImage.h"

#include <stdexcept>

#include "soe/PacketBuffer.h"

namespace assets {

TgaImage TgaImage::parse(const std::vector<uint8_t>& bytes) {
    soe::PacketBuffer buf(bytes.data(), bytes.size());

    uint8_t idLength = buf.readByte();
    uint8_t colorMapType = buf.readByte();
    uint8_t imageType = buf.readByte();
    buf.readUint16(); // colorMapOrigin
    uint16_t colorMapLength = buf.readUint16();
    uint8_t colorMapDepth = buf.readByte();
    buf.readUint16(); // xOrigin
    buf.readUint16(); // yOrigin
    uint16_t width = buf.readUint16();
    uint16_t height = buf.readUint16();
    uint8_t bitsPerPixel = buf.readByte();
    buf.readByte(); // imageDescriptor

    if (colorMapType != 0) {
        throw std::runtime_error("TgaImage: color-mapped TGA not supported");
    }
    if (imageType != 2) {
        throw std::runtime_error("TgaImage: only uncompressed true-color (type 2) is supported");
    }
    if (bitsPerPixel != 24) {
        throw std::runtime_error("TgaImage: only 24bpp is supported");
    }
    if (height != 1) {
        throw std::runtime_error("TgaImage: only single-row (height==1) images are supported");
    }

    buf.readBytes(idLength);
    buf.readBytes(static_cast<size_t>(colorMapLength) * (colorMapDepth / 8));

    TgaImage image;
    image.width = width;
    image.pixels.reserve(width);
    for (uint16_t i = 0; i < width; ++i) {
        // Standard TGA 24bpp pixel order is BGR, not RGB.
        Rgb8 pixel;
        pixel.b = buf.readByte();
        pixel.g = buf.readByte();
        pixel.r = buf.readByte();
        image.pixels.push_back(pixel);
    }
    // A trailing TGA 2.0 footer (26 bytes), if present, is ignored - we
    // only ever need the pixel row.

    return image;
}

} // namespace assets
