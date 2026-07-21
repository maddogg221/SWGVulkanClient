#include "swgproto/StaticObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<StaticObjectBaseline6> StaticObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<StaticObjectBaseline6>(kStaticObjectBaseline6Schema, buf);
}

} // namespace swgproto
