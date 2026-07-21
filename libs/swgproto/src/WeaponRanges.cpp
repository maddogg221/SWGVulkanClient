#include "swgproto/WeaponRanges.h"

namespace swgproto {

WeaponRanges WeaponRanges::parse(soe::PacketBuffer& buf) {
    WeaponRanges result;
    result.weaponObjectId = buf.readUint64();
    result.idealRange = buf.readFloat();
    result.maxRange = buf.readFloat();
    result.pointBlankAccuracy = static_cast<int32_t>(buf.readUint32());
    result.idealAccuracy = static_cast<int32_t>(buf.readUint32());
    result.maxRangeAccuracy = static_cast<int32_t>(buf.readUint32());
    return result;
}

} // namespace swgproto
