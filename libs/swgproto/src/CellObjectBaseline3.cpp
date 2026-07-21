#include "swgproto/CellObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<CellObjectBaseline3> CellObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<CellObjectBaseline3>(kCellObjectBaseline3Schema, buf);
}

} // namespace swgproto
