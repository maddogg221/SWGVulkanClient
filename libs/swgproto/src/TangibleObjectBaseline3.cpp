#include "swgproto/TangibleObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<TangibleObjectBaseline3> TangibleObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<TangibleObjectBaseline3>(kTangibleObjectBaseline3Schema, buf);
}

} // namespace swgproto
