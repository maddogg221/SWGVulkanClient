#include "swgproto/ClientRandomNameResponse.h"

namespace swgproto {

ClientRandomNameResponse ClientRandomNameResponse::parse(soe::PacketBuffer& buf) {
    ClientRandomNameResponse result;
    result.raceIff = buf.readAscii();
    result.name = buf.readUnicode();
    buf.readAscii();  // STF file ("ui"), unused
    buf.readUint32(); // spacer, unused
    buf.readAscii();  // STF variable name ("name_approved"), unused
    return result;
}

} // namespace swgproto
