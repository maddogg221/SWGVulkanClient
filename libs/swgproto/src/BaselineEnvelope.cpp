#include "swgproto/BaselineEnvelope.h"

#include <cctype>
#include <exception>
#include <sstream>

namespace swgproto {

ParseResult<BaselineEnvelope> BaselineEnvelope::parse(soe::PacketBuffer& buf) {
    BaselineEnvelope result;

    try {
        result.objectId = buf.readUint64();
        result.objectType = buf.readUint32();
        result.baselineNumber = buf.readByte();
        result.size = buf.readUint32();
        result.count = buf.readUint16();
    } catch (const std::exception& e) {
        return ParseResult<BaselineEnvelope>::invalid(
            std::string("buffer too short for envelope fields: ") + e.what());
    }

    // `size` covers `count`'s own 2 bytes plus all per-type data, so it can
    // never be smaller than 2 - a smaller value means the field is
    // corrupt/misparsed rather than just "no data", matching this
    // project's established practice of validating wire-supplied sizes.
    if (result.size < 2) {
        return ParseResult<BaselineEnvelope>::invalid(
            "size field " + std::to_string(result.size) +
            " smaller than the minimum possible value (2)");
    }

    return ParseResult<BaselineEnvelope>::ok(std::move(result));
}

std::string objectTypeTag(uint32_t objectType) {
    char bytes[4] = {
        static_cast<char>((objectType >> 24) & 0xFF),
        static_cast<char>((objectType >> 16) & 0xFF),
        static_cast<char>((objectType >> 8) & 0xFF),
        static_cast<char>(objectType & 0xFF),
    };

    for (char ch : bytes) {
        if (!std::isprint(static_cast<unsigned char>(ch))) {
            std::ostringstream oss;
            oss << "0x" << std::hex << objectType;
            return oss.str();
        }
    }

    return std::string(bytes, 4);
}

} // namespace swgproto
