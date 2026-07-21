#include "swgproto/StaticObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<StaticObjectBaseline3> StaticObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<StaticObjectBaseline3>(kStaticObjectBaseline3Schema, buf);
}

} // namespace swgproto
