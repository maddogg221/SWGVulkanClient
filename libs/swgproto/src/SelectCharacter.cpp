#include "swgproto/SelectCharacter.h"

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildSelectCharacter(uint64_t characterId) {
    soe::PacketBuffer buf;
    buf.writeUint16(0x02); // opCount
    buf.writeUint32(soe::MessageHash::compute("SelectCharacter"));
    buf.writeUint64(characterId);
    buf.writeUint32(soe::MessageHash::compute("SWGEmu"));

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
