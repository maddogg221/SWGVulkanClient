#include "swgproto/StartingLocationListMessage.h"

namespace swgproto {

StartingLocationEntry StartingLocationEntry::parse(soe::PacketBuffer& buf) {
    StartingLocationEntry result;
    result.location = buf.readAscii();
    result.planet = buf.readAscii();
    result.x = buf.readFloat();
    result.y = buf.readFloat();
    result.cell = buf.readAscii();
    result.image = buf.readAscii();
    result.description = buf.readAscii();
    result.unknownFlag = buf.readByte();
    return result;
}

StartingLocationListMessage StartingLocationListMessage::parse(soe::PacketBuffer& buf) {
    StartingLocationListMessage result;
    uint32_t count = buf.readUint32();
    result.locations.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        result.locations.push_back(StartingLocationEntry::parse(buf));
    }
    return result;
}

} // namespace swgproto
