#include "swgproto/GuildObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<GuildObjectBaseline3> GuildObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<GuildObjectBaseline3>(kGuildObjectBaseline3Schema, buf);
}

} // namespace swgproto
