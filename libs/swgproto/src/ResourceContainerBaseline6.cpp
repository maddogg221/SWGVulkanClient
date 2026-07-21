#include "swgproto/ResourceContainerBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<ResourceContainerBaseline6> ResourceContainerBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<ResourceContainerBaseline6>(kResourceContainerBaseline6Schema, buf);
}

} // namespace swgproto
