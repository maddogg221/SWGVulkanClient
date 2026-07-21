#include "swgproto/ClientRandomNameRequestPacket.h"

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildClientRandomNameRequestPacket(const std::string& raceTemplate) {
    soe::PacketBuffer buf;
    buf.writeUint16(0x02); // opCount
    buf.writeUint32(soe::MessageHash::compute("ClientRandomNameRequest")); // 0xD6D1B6D1
    buf.writeAscii(raceTemplate);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
