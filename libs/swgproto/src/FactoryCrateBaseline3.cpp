#include "swgproto/FactoryCrateBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<FactoryCrateBaseline3> FactoryCrateBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<FactoryCrateBaseline3>(kFactoryCrateBaseline3Schema, buf);
}

} // namespace swgproto
