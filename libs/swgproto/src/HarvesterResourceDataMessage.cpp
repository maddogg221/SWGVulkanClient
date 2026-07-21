#include "swgproto/HarvesterResourceDataMessage.h"

namespace swgproto {

HarvesterResourceDataEntry HarvesterResourceDataEntry::parse(soe::PacketBuffer& buf) {
    HarvesterResourceDataEntry result;
    result.resourceSpawnId = buf.readUint64();
    result.name = buf.readAscii();
    result.resourceType = buf.readAscii();
    result.densityPercent = buf.readByte();
    return result;
}

HarvesterResourceDataMessage HarvesterResourceDataMessage::parse(soe::PacketBuffer& buf) {
    HarvesterResourceDataMessage result;
    result.harvesterObjectId = buf.readUint64();
    uint32_t count = buf.readUint32();
    result.resources.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        result.resources.push_back(HarvesterResourceDataEntry::parse(buf));
    }
    return result;
}

} // namespace swgproto
