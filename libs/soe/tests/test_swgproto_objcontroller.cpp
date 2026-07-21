// Tests for Phase 4's first four ObjControllerMessage sub-types: Animation,
// PostureMessage, DataTransform, DataTransformWithParent. The synthetic
// hand-built-buffer cases pin the field order/count that's easiest to get
// wrong on a first pass (in particular DataTransform/DataTransformWithParent's
// real field-count ambiguity - see DataTransform.h - where Core3's own
// client-side parser and its server-side outgoing constructor genuinely
// disagree on field count; these tests pin the constructor's shorter list,
// which is what a passive client actually receives). The real-byte fixtures
// below (captured live from Finalizer, character "Kalda Ulzo" on Tatooine,
// via the established temporary hex-dump method) are the empirical
// confirmation - independently re-verified field-by-field (including precise
// float decoding via a BitConverter cross-check, not hand IEEE-754 math)
// before being pinned, with zero leftover bytes on every one.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/Animation.h"
#include "swgproto/DataTransform.h"
#include "swgproto/DataTransformWithParent.h"
#include "swgproto/PostureMessage.h"

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

TEST_CASE("Animation::parse - synthetic") {
    soe::PacketBuffer buf;
    buf.writeAscii("dance_flourish");

    auto msg = Animation::parse(buf);
    CHECK(msg.animationName == "dance_flourish");
    CHECK(buf.remaining() == 0);
}

TEST_CASE("Animation::parse - real payload (Kalda Ulzo, Finalizer, Tatooine)") {
    // Captured 2026-07-15 via a temporary hex-dump patch. 16 bytes, zero
    // leftover.
    auto buf = bufferFromHex("0e 00 73 6b 69 6c 6c 5f 61 63 74 69 6f 6e 5f 31");

    auto msg = Animation::parse(buf);
    CHECK(msg.animationName == "skill_action_1");
    CHECK(buf.remaining() == 0);
}

TEST_CASE("PostureMessage::parse - synthetic") {
    soe::PacketBuffer buf;
    buf.writeByte(8);  // sitting
    buf.writeByte(1);

    auto msg = PostureMessage::parse(buf);
    CHECK(msg.posture == 8);
    CHECK(msg.unknownFlag == 1);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("PostureMessage::parse - real payload (Kalda Ulzo, Finalizer, Tatooine)") {
    // Captured 2026-07-15. 2 bytes, zero leftover. posture=0 (upright).
    auto buf = bufferFromHex("00 01");

    auto msg = PostureMessage::parse(buf);
    CHECK(msg.posture == 0);
    CHECK(msg.unknownFlag == 1);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("DataTransform::parse - field order and count") {
    soe::PacketBuffer buf;
    buf.writeUint32(42);      // counter
    buf.writeFloat(0.0f);     // directionX
    buf.writeFloat(-0.267f);  // directionY
    buf.writeFloat(0.0f);     // directionZ
    buf.writeFloat(0.9637f);  // directionW
    buf.writeFloat(3508.6f);  // x
    buf.writeFloat(5.52f);    // y (height)
    buf.writeFloat(-4923.4f); // z
    buf.writeFloat(0.0f);     // speed

    auto msg = DataTransform::parse(buf);
    CHECK(msg.counter == 42);
    CHECK(msg.directionX == doctest::Approx(0.0f));
    CHECK(msg.directionY == doctest::Approx(-0.267f));
    CHECK(msg.directionZ == doctest::Approx(0.0f));
    CHECK(msg.directionW == doctest::Approx(0.9637f));
    CHECK(msg.x == doctest::Approx(3508.6f));
    CHECK(msg.y == doctest::Approx(5.52f));
    CHECK(msg.z == doctest::Approx(-4923.4f));
    CHECK(msg.speed == doctest::Approx(0.0f));
    // 36 bytes total (uint32 + 8 floats) - this is the field-count
    // resolution: Core3's client-side parser reads a 2nd leading uint32
    // (moveCount) the server's own outgoing constructor never sends: only
    // one is present here, matching what real live traffic actually
    // contains (see the real-payload test below).
    CHECK(buf.remaining() == 0);
}

TEST_CASE("DataTransform::parse - real payload (Kalda Ulzo, Finalizer, Tatooine)") {
    // Captured 2026-07-15 via a temporary hex-dump patch, from an idle
    // NPC's synchronize confirmation (not this connection's own character -
    // see DataTransform.h's corrected-live note). 36 bytes, zero leftover -
    // empirically confirms the field-count resolution above against real
    // traffic, not just a synthetic assumption.
    auto buf = bufferFromHex(
        "97 02 00 00 00 00 00 00 45 b9 88 be 00 00 00 00 1e b4 76 3f 3a 4a 5b 45 00 b0 b0 40 "
        "e7 da 99 c5 00 00 00 00");

    auto msg = DataTransform::parse(buf);
    CHECK(msg.counter == 663);
    CHECK(msg.directionX == doctest::Approx(0.0f));
    CHECK(msg.directionY == doctest::Approx(-0.2670385f));
    CHECK(msg.directionZ == doctest::Approx(0.0f));
    CHECK(msg.directionW == doctest::Approx(0.9636859f));
    CHECK(msg.x == doctest::Approx(3508.639f));
    CHECK(msg.y == doctest::Approx(5.521484f));
    CHECK(msg.z == doctest::Approx(-4923.363f));
    CHECK(msg.speed == doctest::Approx(0.0f));
    CHECK(buf.remaining() == 0);
}

TEST_CASE("DataTransformWithParent::parse - parentId precedes direction") {
    soe::PacketBuffer buf;
    buf.writeUint32(7);                  // counter
    buf.writeUint64(1082877ULL);         // parentId
    buf.writeFloat(0.0f);                // directionX
    buf.writeFloat(1.0f);                // directionY
    buf.writeFloat(0.0f);                // directionZ
    buf.writeFloat(0.0f);                // directionW
    buf.writeFloat(-8.66f);              // x
    buf.writeFloat(-0.9f);               // y (height)
    buf.writeFloat(-2.13f);              // z
    buf.writeFloat(0.0f);                // speed

    auto msg = DataTransformWithParent::parse(buf);
    CHECK(msg.counter == 7);
    CHECK(msg.parentId == 1082877ULL);
    CHECK(msg.directionX == doctest::Approx(0.0f));
    CHECK(msg.directionY == doctest::Approx(1.0f));
    CHECK(msg.directionZ == doctest::Approx(0.0f));
    CHECK(msg.directionW == doctest::Approx(0.0f));
    CHECK(msg.x == doctest::Approx(-8.66f));
    CHECK(msg.y == doctest::Approx(-0.9f));
    CHECK(msg.z == doctest::Approx(-2.13f));
    CHECK(msg.speed == doctest::Approx(0.0f));
    // 44 bytes total (uint32 + uint64 + 8 floats).
    CHECK(buf.remaining() == 0);
}

TEST_CASE("DataTransformWithParent::parse - real payload (Kalda Ulzo, Finalizer, Tatooine)") {
    // Captured 2026-07-15 via a temporary hex-dump patch. 44 bytes, zero
    // leftover. parentId matches the same cell ID (1082877) independently
    // seen in Phase 3's UpdateTransformWithParentMessage real-payload test -
    // a real cross-message-type consistency check, not a coincidence.
    auto buf = bufferFromHex(
        "01 07 09 00 fd 85 10 00 00 00 00 00 00 00 00 00 00 00 80 3f 00 00 00 00 2e bd 3b b3 "
        "19 9a 0a c1 66 66 66 bf e5 6c 08 c0 00 00 00 00");

    auto msg = DataTransformWithParent::parse(buf);
    CHECK(msg.counter == 591617);
    CHECK(msg.parentId == 1082877ULL);
    CHECK(msg.directionX == doctest::Approx(0.0f));
    CHECK(msg.directionY == doctest::Approx(1.0f));
    CHECK(msg.directionZ == doctest::Approx(0.0f));
    CHECK(msg.directionW == doctest::Approx(-4.371139e-08f));
    CHECK(msg.x == doctest::Approx(-8.662621f));
    CHECK(msg.y == doctest::Approx(-0.9f));
    CHECK(msg.z == doctest::Approx(-2.131646f));
    CHECK(msg.speed == doctest::Approx(0.0f));
    CHECK(buf.remaining() == 0);
}
