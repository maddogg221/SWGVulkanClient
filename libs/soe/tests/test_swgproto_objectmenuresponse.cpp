// Test for ObjectMenuResponse (header2=0x147), the right-click radial menu
// itself. SYNTHETIC ONLY - no real capture exists yet. This message had a
// real, previously-unresolved wire-format ambiguity (not just "hard to
// trigger" like most other SUI-adjacent sub-types): Core3's own
// ObjectMenuResponse.h patches a size field via insertInt(46, listSize) at
// what looked like the wrong absolute offset given this project's own
// envelope accounting (36, not 46). Resolved 2026-07-18 by tracing past
// what this project had previously looked at: Core3's insertInt(46, ...) is
// measured from ITS OWN raw server-side buffer, but this project's buffer
// has already had 10 bytes stripped before ObjControllerMessage::parse()
// ever runs (4 bytes of SOE Data Channel opcode+sequence, stripped by
// SoeSession::receiveMessages() itself; a further 2-byte opCount + 4-byte
// hash, stripped by soe::MessageDispatcher) - 46 - 10 = 36, matching this
// project's own accounting exactly. The "4 missing bytes" were never
// missing; they were measured in two different buffers' coordinate systems
// and compared as if identical. See SESSION_LOG.md for the full trace,
// including independent corroboration from Core3's own
// ZoneClientSessionImplementation.cpp packet-logging code, which explicitly
// branches on this same 4-byte prefix.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjectMenuResponse.h"

using namespace swgproto;

namespace {

soe::PacketBuffer bufferFromHex(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(hex);
    std::string token;
    while (iss >> token) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
    }
    return soe::PacketBuffer(bytes.data(), bytes.size());
}

} // namespace

TEST_CASE("ObjectMenuResponse::parse - synthetic two-item radial menu") {
    // target=100, player=200, listSize=2,
    // item[0]: itemIndex=1 parentIndex=0 radialId=0x14 callback=3 text="Use",
    // item[1]: itemIndex=2 parentIndex=1 radialId=0x15 callback=3 text="",
    // trailing count=5.
    auto buf = bufferFromHex(
        "64 00 00 00 00 00 00 00 "
        "c8 00 00 00 00 00 00 00 "
        "02 00 00 00 "
        "01 00 14 03 03 00 00 00 55 00 73 00 65 00 "
        "02 01 15 03 00 00 00 00 "
        "05");

    auto msg = ObjectMenuResponse::parse(buf);
    CHECK(msg.target == 100ULL);
    CHECK(msg.player == 200ULL);
    CHECK(msg.listSize == 2);
    REQUIRE(msg.items.size() == 2);

    CHECK(msg.items[0].itemIndex == 1);
    CHECK(msg.items[0].parentIndex == 0);
    CHECK(msg.items[0].radialId == 0x14);
    CHECK(msg.items[0].callback == 3);
    CHECK(msg.items[0].text == u"Use");

    CHECK(msg.items[1].itemIndex == 2);
    CHECK(msg.items[1].parentIndex == 1);
    CHECK(msg.items[1].radialId == 0x15);
    CHECK(msg.items[1].callback == 3);
    CHECK(msg.items[1].text.empty());

    CHECK(msg.count == 5);
    CHECK(buf.remaining() == 0);
}
