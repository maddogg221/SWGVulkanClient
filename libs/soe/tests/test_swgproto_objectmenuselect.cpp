// Permanent regression test for ObjectMenuSelect - the outbound reply to a
// real right-click radial menu, discovered via a real Phase 17 packet
// capture against Naritus (2026-07-20). This test pins the builder's output
// against the ACTUAL bytes the real official client sent when selecting
// "@elevator_text:up" / "@elevator_text:down" from a real elevator
// terminal's radial menu - confirmed by cross-referencing the same
// session's ObjectMenuResponse, which listed radialId 198/199 as
// "@elevator_text:up"/"down" for those exact target objectIds. Also pins
// the message hash itself: it's a raw hex literal Core3 registers directly
// (ZonePacketHandler.cpp: registerObject<ObjectMenuSelectCallback>
// (0x7CA18726)), NOT hashCode("ObjectMenuSelect") - the two differ, which a
// first attempt at this test caught by comparing full raw bytes against the
// real capture instead of only re-parsing the builder's own output.
#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjectMenuSelect.h"

using namespace swgproto;

TEST_CASE("buildObjectMenuSelect - wire shape matches a real captured elevator 'up' selection") {
    const uint64_t objectId = 281474994287369ULL;
    const uint8_t radialId = 198;

    auto bytes = buildObjectMenuSelect(objectId, radialId);

    // Real captured bytes (opCount+hash+objectId+radialId), extracted from a
    // real client->server SOE MultiPacket (0x0300) bundle in the Phase 17
    // capture, decrypted and reassembled the same way SoeSession does live.
    const std::vector<uint8_t> realCaptured = {0x03, 0x00, 0x26, 0x87, 0xa1, 0x7c, 0x09, 0x33,
                                                0x0c, 0x01, 0x00, 0x00, 0x01, 0x00, 0xc6};
    CHECK(bytes == realCaptured);

    soe::PacketBuffer buf(bytes.data(), bytes.size());
    CHECK(buf.readUint16() == 0x03); // opCount
    CHECK(buf.readUint32() == kObjectMenuSelectHash);
    CHECK(buf.readUint64() == objectId);
    CHECK(buf.readByte() == radialId);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("buildObjectMenuSelect - wire shape matches a real captured elevator 'down' selection") {
    const uint64_t objectId = 281474994287370ULL;
    const uint8_t radialId = 199;

    auto bytes = buildObjectMenuSelect(objectId, radialId);

    const std::vector<uint8_t> realCaptured = {0x03, 0x00, 0x26, 0x87, 0xa1, 0x7c, 0x0a, 0x33,
                                                0x0c, 0x01, 0x00, 0x00, 0x01, 0x00, 0xc7};
    CHECK(bytes == realCaptured);
}
