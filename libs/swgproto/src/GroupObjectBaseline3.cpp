#include "swgproto/GroupObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<GroupObjectBaseline3> GroupObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<GroupObjectBaseline3>(kGroupObjectBaseline3Schema, buf);
}

} // namespace swgproto
