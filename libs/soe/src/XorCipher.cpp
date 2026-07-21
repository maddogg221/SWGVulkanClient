#include "soe/XorCipher.h"

namespace soe {
namespace {

uint32_t readLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void writeLE32(uint8_t* p, uint32_t val) {
    p[0] = static_cast<uint8_t>(val & 0xFF);
    p[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

} // namespace

void XorCipher::encrypt(PacketBuffer& buffer, uint32_t crcSeed) {
    uint8_t* pData = buffer.data();
    int length = static_cast<int>(buffer.size());
    if (length <= 0) {
        return;
    }

    uint32_t seed = crcSeed;
    int offset;
    if (pData[0] == 0x00) {
        length -= 4;
        offset = 2;
    } else {
        length -= 3;
        offset = 1;
    }
    if (length <= 0) {
        return;
    }

    int blockCount = length / 4;
    int byteCount = length % 4;

    size_t pos = static_cast<size_t>(offset);
    for (int i = 0; i < blockCount; ++i) {
        uint32_t block = readLE32(pData + pos) ^ seed;
        writeLE32(pData + pos, block);
        seed = block; // chain off the resulting ciphertext
        pos += 4;
    }

    for (int i = 0; i < byteCount; ++i) {
        pData[pos] ^= static_cast<uint8_t>(seed & 0xFF);
        pos++;
    }
}

void XorCipher::decrypt(PacketBuffer& buffer, uint32_t crcSeed) {
    uint8_t* pData = buffer.data();
    int length = static_cast<int>(buffer.size());
    if (length < 3) {
        return;
    }

    uint32_t seed = crcSeed;
    int offset;
    if (pData[0] == 0x00) {
        length -= 4;
        offset = 2;
    } else {
        length -= 3;
        offset = 1;
    }
    if (length <= 0) {
        return;
    }

    int blockCount = length / 4;
    int byteCount = length % 4;

    size_t pos = static_cast<size_t>(offset);
    for (int i = 0; i < blockCount; ++i) {
        uint32_t cipher = readLE32(pData + pos);
        uint32_t plain = cipher ^ seed;
        writeLE32(pData + pos, plain);
        seed = cipher; // chain off the original ciphertext
        pos += 4;
    }

    for (int i = 0; i < byteCount; ++i) {
        pData[pos] ^= static_cast<uint8_t>(seed & 0xFF);
        pos++;
    }
}

} // namespace soe
