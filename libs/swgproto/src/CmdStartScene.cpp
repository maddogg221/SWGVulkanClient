#include "swgproto/CmdStartScene.h"

namespace swgproto {

CmdStartScene CmdStartScene::parse(soe::PacketBuffer& buf) {
    CmdStartScene result;

    buf.readByte(); // unknown, always 0
    result.selfObjectId = buf.readUint64();
    result.terrainName = buf.readAscii();
    result.x = buf.readFloat();
    result.y = buf.readFloat(); // Core3's Vector3::getZ() - see CmdStartScene.h
    result.z = buf.readFloat(); // Core3's Vector3::getY() - see CmdStartScene.h
    result.templateFile = buf.readAscii();
    result.galacticTime = buf.readUint64();

    return result;
}

} // namespace swgproto
