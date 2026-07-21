#include "swgproto/ClientCreateCharacterSuccess.h"

namespace swgproto {

ClientCreateCharacterSuccess ClientCreateCharacterSuccess::parse(soe::PacketBuffer& buf) {
    ClientCreateCharacterSuccess result;
    result.objectId = buf.readUint64();
    return result;
}

} // namespace swgproto
