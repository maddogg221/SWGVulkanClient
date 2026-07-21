#include "swgproto/ObjectMenuResponse.h"

namespace swgproto {

RadialMenuItemEntry RadialMenuItemEntry::parse(soe::PacketBuffer& buf) {
    RadialMenuItemEntry result;
    result.itemIndex = buf.readByte();
    result.parentIndex = buf.readByte();
    result.radialId = buf.readByte();
    result.callback = buf.readByte();
    result.text = buf.readUnicode();
    return result;
}

ObjectMenuResponse ObjectMenuResponse::parse(soe::PacketBuffer& buf) {
    ObjectMenuResponse result;
    result.target = buf.readUint64();
    result.player = buf.readUint64();
    result.listSize = buf.readUint32();
    result.items.reserve(result.listSize);
    for (uint32_t i = 0; i < result.listSize; ++i) {
        result.items.push_back(RadialMenuItemEntry::parse(buf));
    }
    result.count = buf.readByte();
    return result;
}

} // namespace swgproto
