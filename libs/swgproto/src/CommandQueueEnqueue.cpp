#include "swgproto/CommandQueueEnqueue.h"

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace {

// Core3 registers commands by their lowercased name (see header comment) -
// lowercase defensively here so callers don't need to remember that.
std::string toLowerAscii(const std::string& s) {
    std::string result = s;
    for (char& ch : result) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return result;
}

} // namespace

namespace swgproto {

std::vector<uint8_t> buildCommandQueueEnqueue(uint64_t objectId, uint32_t actionCount,
                                               const std::string& commandName, uint64_t targetId,
                                               const std::u16string& arguments) {
    soe::PacketBuffer buf;

    // Shared ObjectControllerMessage envelope. Unlike the server->client
    // direction (this project's own ObjControllerMessage::parse(), which
    // consumes a trailing unused int32 after objectId), the client->server
    // envelope is only header1+header2+objectId - confirmed by live-testing
    // against Naritus: including that extra field shifted every downstream
    // body field by 4 bytes (observed server-side: actionCRC=1 (our
    // actionCount), actionCount=32 (our remainingSize), targetID=our actionCRC
    // value read as a uint64 - an exact match once traced by hand).
    buf.writeUint16(0x05); // opCount
    buf.writeUint32(soe::MessageHash::compute("ObjControllerMessage"));
    buf.writeUint32(0x0B);  // header1
    buf.writeUint32(0x116); // header2 (CommandQueueEnqueue)
    buf.writeUint64(objectId);

    // CommandQueueEnqueue's own body. `size` isn't read for anything by the
    // server (see header comment) - filled with the actual remaining byte
    // count as a defensible value, not because it's checked.
    uint32_t remainingSize =
        4 /* actionCount */ + 4 /* actionCRC */ + 8 /* targetId */ +
        4 /* arguments length prefix */ + static_cast<uint32_t>(arguments.size()) * 2;
    buf.writeUint32(remainingSize);
    buf.writeUint32(actionCount);
    buf.writeUint32(soe::MessageHash::compute(toLowerAscii(commandName)));
    buf.writeUint64(targetId);
    buf.writeUnicode(arguments);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
