#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's ObjectMenuResponse.h:
// ObjectControllerMessage(player->getObjectID(), 0x0B, 0x147).
constexpr uint32_t kObjectMenuResponseControllerType = 0x147;

// One entry in the flat radial-menu-item list, from ObjectMenuResponse.h's
// addRadialOption(): insertByte(itemIndex) + insertByte(parentIndex) +
// insertByte(radialID) + insertByte(callback) + insertUnicode(text). Written
// via a recursive depth-first walk (insertRadialItemToMessage()), but each
// entry carries its own parentIndex, so the wire format itself is a flat
// list - no nested framing needed to decode it, a tree can be reconstructed
// client-side from parentIndex references if ever needed.
struct RadialMenuItemEntry {
    uint8_t itemIndex = 0;
    uint8_t parentIndex = 0;
    uint8_t radialId = 0;
    uint8_t callback = 0;
    std::u16string text;

    static RadialMenuItemEntry parse(soe::PacketBuffer& buf);
};

// The right-click radial menu itself - real wire-format ambiguity this
// project was stuck on since Phase 4 Step 12, resolved 2026-07-18 by tracing
// past what this project had already looked at: ObjectMenuResponse.h's
// finish() does insertInt(46, listSize), patching 4 bytes at Core3's own
// RAW server-side buffer offset 46 - but this project's own buffer never
// sees those first 10 bytes (SoeSession::receiveMessages() already strips
// the 4-byte SOE Data Channel opcode+sequence prefix - see SoeSession.cpp's
// buf.readUint16()/readUint16BE() calls just after buf.removeLastBytes(3) -
// and soe::MessageDispatcher strips a further 2-byte opCount + 4-byte hash
// before ObjControllerDispatcher::dispatch() ever runs). 46 - 10 = 36,
// exactly matching this project's own envelope accounting (the shared
// 20-byte ObjControllerMessage envelope + 8-byte target + 8-byte player =
// offset 36) - the "4 missing bytes" were never actually missing, just
// measured in two different buffers' coordinate systems and compared as if
// they were the same number. See SESSION_LOG.md for the full trace.
//
// listSize itself is redundant with the parsed vector's own size() once
// decoded - kept as a separate field anyway (rather than silently dropped)
// since it's a real value on the wire and a mismatch against items.size()
// would be a genuine signal something upstream is wrong, not noise.
struct ObjectMenuResponse {
    uint64_t target = 0;
    uint64_t player = 0;
    uint32_t listSize = 0;
    std::vector<RadialMenuItemEntry> items;
    // Trailing byte after the item list (ObjectMenuResponse.h's finish():
    // insertInt(46, listSize) then insertByte(count)) - Core3's own
    // constructor comments/call sites don't make its purpose obvious
    // (every real call site found passes counter=0); kept as a raw field
    // rather than guessed at.
    uint8_t count = 0;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static ObjectMenuResponse parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
