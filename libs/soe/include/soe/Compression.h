#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace soe {

// zlib deflate/inflate wrapper matching Core3's single-shot Z_FINISH call
// pattern (BaseProtocol::compress/decompress). Pure byte-buffer transform -
// packet framing (the compression-flag byte, opcode-offset handling, etc.)
// is an SoeSession-level concern, not this file's. Unlike PacketCrc/
// XorCipher, this does not need to be bit-exact with Core3's own output -
// any valid zlib deflate stream is interoperable with any zlib inflate.
class Compression {
public:
    // Deflates `input` at the default compression level. The result may be
    // larger than the input for incompressible/short data - callers decide
    // whether it was worth using based on the resulting size (Core3 skips
    // compression whenever it wouldn't shrink the packet).
    static std::vector<uint8_t> compress(const uint8_t* input, size_t inputLen);

    // Inflates `input`. `expectedMaxSize` must be large enough to hold the
    // fully decompressed output.
    static std::vector<uint8_t> decompress(const uint8_t* input, size_t inputLen, size_t expectedMaxSize);
};

} // namespace soe
