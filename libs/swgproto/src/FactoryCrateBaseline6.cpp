#include "swgproto/FactoryCrateBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<FactoryCrateBaseline6> FactoryCrateBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<FactoryCrateBaseline6>(kFactoryCrateBaseline6Schema, buf);
}

} // namespace swgproto
