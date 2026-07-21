#include "swgproto/CellObjectBaseline6.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<CellObjectBaseline6> CellObjectBaseline6::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<CellObjectBaseline6>(kCellObjectBaseline6Schema, buf);
}

} // namespace swgproto
