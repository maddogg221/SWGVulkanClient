// Test for SceneDestroyObject (hash 0x4D45D504) - the general "remove this
// object from the world" signal, the direct counterpart to
// SceneCreateObjectByCrc. Confirmed live 2026-07-18: destroyed a real
// FactoryCrate (objectId=281474993942554, "Generic Items Crate (System
// Generated)") via the real "serverdestroyobject" admin command and
// received exactly this decoded shape back with zero exceptions - name and
// hash both independently verified (SceneDestroyObject.h's own header
// comment traces both the real server-side trigger sites and the
// hashCode("SceneDestroyObject") == 0x4D45D504 match against the reference
// client's own dispatch table). Bytes below are precisely reconstructed
// from that real live-confirmed objectId/hyperspacing pair (the raw hex
// wasn't captured verbatim - only the decoded fields were printed - but
// the wire layout itself (uint64 + byte) has zero ambiguity).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/SceneDestroyObject.h"

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

TEST_CASE("SceneDestroyObject::parse - real objectId, live-confirmed via serverdestroyobject") {
    auto buf = bufferFromHex("1a f0 06 01 00 00 01 00 00");

    auto msg = SceneDestroyObject::parse(buf);
    CHECK(msg.objectId == 281474993942554ULL);
    CHECK(msg.hyperspacing == false);
    CHECK(buf.remaining() == 0);
}
