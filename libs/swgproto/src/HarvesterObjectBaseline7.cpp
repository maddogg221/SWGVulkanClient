#include "swgproto/HarvesterObjectBaseline7.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<HarvesterObjectBaseline7> HarvesterObjectBaseline7::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<HarvesterObjectBaseline7>(kHarvesterObjectBaseline7Schema, buf);
}

} // namespace swgproto
