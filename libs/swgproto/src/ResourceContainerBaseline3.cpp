#include "swgproto/ResourceContainerBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<ResourceContainerBaseline3> ResourceContainerBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<ResourceContainerBaseline3>(kResourceContainerBaseline3Schema, buf);
}

} // namespace swgproto
