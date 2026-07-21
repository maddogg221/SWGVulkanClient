#include "swgproto/IntangibleObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<IntangibleObjectBaseline3> IntangibleObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<IntangibleObjectBaseline3>(kIntangibleObjectBaseline3Schema, buf);
}

} // namespace swgproto
