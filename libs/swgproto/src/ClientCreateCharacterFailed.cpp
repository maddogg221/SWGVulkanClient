#include "swgproto/ClientCreateCharacterFailed.h"

namespace swgproto {

ClientCreateCharacterFailed ClientCreateCharacterFailed::parse(soe::PacketBuffer& buf) {
    ClientCreateCharacterFailed result;
    buf.readUint32();  // unused Unicode-String-in-place-of-stf-file placeholder
    buf.readAscii();   // STF file ("ui"), unused
    buf.readUint32();  // spacer, unused
    result.errorString = buf.readAscii();
    return result;
}

} // namespace swgproto
