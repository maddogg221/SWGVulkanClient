#include "swgproto/DataTransform.h"

#include "soe/MessageHash.h"

namespace swgproto {

DataTransform DataTransform::parse(soe::PacketBuffer& buf) {
    DataTransform result;

    result.counter = buf.readUint32();
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

std::vector<uint8_t> buildDataTransform(uint64_t objectId, uint32_t timeStamp,
                                         uint32_t moveCount, float dirX, float dirY,
                                         float dirZ, float dirW, float x, float y, float z,
                                         float speed) {
    soe::PacketBuffer buf;

    // Shared ObjectControllerMessage envelope - same proven shape as
    // buildCommandQueueEnqueue() (see that function's own comment): no
    // trailing unused field on the client->server direction.
    buf.writeUint16(0x05); // opCount
    buf.writeUint32(soe::MessageHash::compute("ObjControllerMessage"));
    buf.writeUint32(0x0B);                        // header1
    buf.writeUint32(kDataTransformControllerType); // header2
    buf.writeUint64(objectId);

    buf.writeUint32(timeStamp);
    buf.writeUint32(moveCount);
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
