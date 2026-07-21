#include "swgproto/DataTransformWithParent.h"

#include "soe/MessageHash.h"

namespace swgproto {

DataTransformWithParent DataTransformWithParent::parse(soe::PacketBuffer& buf) {
    DataTransformWithParent result;

    result.counter = buf.readUint32();
    result.parentId = buf.readUint64();
    result.directionX = buf.readFloat();
    result.directionY = buf.readFloat();
    result.directionZ = buf.readFloat();
    result.directionW = buf.readFloat();
    result.x = buf.readFloat();
    result.y = buf.readFloat();
    result.z = buf.readFloat();
    result.speed = buf.readFloat();

    return result;
}

std::vector<uint8_t> buildDataTransformWithParent(uint64_t objectId, uint32_t timeStamp,
                                                    uint32_t moveCount, uint64_t parentId,
                                                    float dirX, float dirY, float dirZ,
                                                    float dirW, float x, float y, float z,
                                                    float speed) {
    soe::PacketBuffer buf;

    // Shared ObjectControllerMessage envelope - same proven shape as
    // buildDataTransform()/buildCommandQueueEnqueue(): no trailing unused
    // field on the client->server direction.
    buf.writeUint16(0x05); // opCount
    buf.writeUint32(soe::MessageHash::compute("ObjControllerMessage"));
    buf.writeUint32(0x0B);                                    // header1
    buf.writeUint32(kDataTransformWithParentControllerType);  // header2
    buf.writeUint64(objectId);

    buf.writeUint32(timeStamp);
    buf.writeUint32(moveCount);
    buf.writeUint64(parentId);
    buf.writeFloat(dirX);
    buf.writeFloat(dirY);
    buf.writeFloat(dirZ);
    buf.writeFloat(dirW);
    buf.writeFloat(x);
    buf.writeFloat(y);
    buf.writeFloat(z);
    buf.writeFloat(speed);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
