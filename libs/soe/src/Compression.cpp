#include "soe/Compression.h"

#include <stdexcept>

#include <zlib.h>

namespace soe {
namespace {

// Real SOE packets are capped at a few hundred bytes (engine3's own
// Packet::RAW_MAX_SIZE is 496) and our own UDP receive buffer is capped at
// 2048 bytes, so anything claiming a size beyond this is definitely
// malformed/corrupt input, not a legitimate (if unusually large) packet.
constexpr size_t kMaxReasonableSize = 65536;

} // namespace

std::vector<uint8_t> Compression::compress(const uint8_t* input, size_t inputLen) {
    if (inputLen > kMaxReasonableSize) {
        throw std::invalid_argument("Compression::compress: inputLen exceeds reasonable bounds");
    }

    z_stream stream{};
    if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Compression::compress: deflateInit failed");
    }

    uLong bound = deflateBound(&stream, static_cast<uLong>(inputLen));
    std::vector<uint8_t> output(bound);

    stream.next_in = const_cast<Bytef*>(input);
    stream.avail_in = static_cast<uInt>(inputLen);
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    int result = deflate(&stream, Z_FINISH);
    uLong totalOut = stream.total_out;
    deflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("Compression::compress: deflate did not finish the stream");
    }

    output.resize(totalOut);
    return output;
}

std::vector<uint8_t> Compression::decompress(const uint8_t* input, size_t inputLen, size_t expectedMaxSize) {
    if (inputLen == 0 || inputLen > kMaxReasonableSize) {
        throw std::invalid_argument("Compression::decompress: inputLen out of reasonable range");
    }
    if (expectedMaxSize == 0 || expectedMaxSize > kMaxReasonableSize) {
        throw std::invalid_argument("Compression::decompress: expectedMaxSize out of reasonable range");
    }

    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error("Compression::decompress: inflateInit failed");
    }

    std::vector<uint8_t> output(expectedMaxSize);

    stream.next_in = const_cast<Bytef*>(input);
    stream.avail_in = static_cast<uInt>(inputLen);
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    int result = inflate(&stream, Z_FINISH);
    uLong totalOut = stream.total_out;
    inflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("Compression::decompress: inflate did not finish the stream");
    }

    output.resize(totalOut);
    return output;
}

} // namespace soe
