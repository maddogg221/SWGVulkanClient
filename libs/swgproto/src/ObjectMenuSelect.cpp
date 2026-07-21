#include "swgproto/ObjectMenuSelect.h"

#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildObjectMenuSelect(uint64_t objectId, uint8_t radialId) {
    soe::PacketBuffer buf;
    buf.writeUint16(0x03); // opCount - confirmed from a real capture (see header comment)
    buf.writeUint32(kObjectMenuSelectHash);
    buf.writeUint64(objectId);
    buf.writeByte(radialId);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
