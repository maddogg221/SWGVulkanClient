#include "swgproto/TangibleObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<TangibleObjectBaseline6> TangibleObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<TangibleObjectBaseline6>(kTangibleObjectBaseline6Schema, buf);
}

} // namespace swgproto
