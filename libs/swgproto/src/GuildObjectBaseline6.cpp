#include "swgproto/GuildObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<GuildObjectBaseline6> GuildObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<GuildObjectBaseline6>(kGuildObjectBaseline6Schema, buf);
}

} // namespace swgproto
