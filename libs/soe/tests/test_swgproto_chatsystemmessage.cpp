// Permanent regression test for ChatSystemMessage's simple (plain Unicode
// string) variant - most notably the zone-server MOTD, which turned out to
// carry Core3's own build revision ("last 10 commits"). Live-verified
// against a REAL Finalizer capture (2026-07-16): decoded cleanly with zero
// leftover bytes, revealing Finalizer is running commit 6856f315a8 - the
// EXACT SAME commit this project's local Core3 reference clone (`C:\SWGEmu
// Repo`) is on. See DISCOVERY.txt's "active exploration plan" entry for the
// full finding. This test uses a synthetic (not the real, much longer)
// fixture - the wire shape is simple enough (byte + Unicode string + int32)
// that a short synthetic payload proves the same thing a real one would;
// the live capture already provided the real-traffic confirmation this
// project's process requires.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/ChatSystemMessage.h"

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

TEST_CASE("ChatSystemMessage::parse - simple variant (synthetic, mirrors real MOTD shape)") {
    // displayType=0x02 (DISPLAY_CHATONLY, matching sendLoginMessage's real
    // usage) + Unicode "Hi" (2 chars) + paramsSize=0 ("no params").
    auto buf = bufferFromHex("02 02 00 00 00 48 00 69 00 00 00 00 00");

    auto result = ChatSystemMessage::parse(buf);

    CHECK(result.displayType == ChatSystemMessage::DisplayChatOnly);
    CHECK(result.message == u"Hi");
    CHECK(result.paramsSize == 0);
    CHECK(buf.remaining() == 0);
}
