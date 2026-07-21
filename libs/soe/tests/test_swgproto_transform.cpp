// Tests for the first movement/position message types (Phase 3's first
// step): UpdateTransformMessage/UpdateTransformWithParentMessage. The
// synthetic hand-built-buffer cases pin down the two things most likely to
// get silently transposed on a first pass at this new wire shape: the
// X,Z,Y (not X,Y,Z) field order, and the /4 vs /8 scale factor difference
// between the two message types. The real-byte fixtures below (captured
// live from Finalizer, character "Kalda Ulzo" on Tatooine, via the
// established temporary hex-dump method) are the empirical confirmation -
// independently re-verified field-by-field before being pinned, matching
// the live console output exactly with zero leftover bytes.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/UpdateTransformMessage.h"
#include "swgproto/UpdateTransformWithParentMessage.h"

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

TEST_CASE("UpdateTransformMessage::parse - scale and field order") {
    soe::PacketBuffer buf;
    buf.writeUint64(281479009480346ULL); // objectId
    buf.writeUint16(static_cast<uint16_t>(static_cast<int16_t>(40)));  // x: 40/4 = 10.0
    buf.writeUint16(static_cast<uint16_t>(static_cast<int16_t>(-20))); // y (height): -20/4 = -5.0
    buf.writeUint16(static_cast<uint16_t>(static_cast<int16_t>(8)));   // z: 8/4 = 2.0
    buf.writeUint32(42);                                               // movementCounter
    buf.writeByte(static_cast<uint8_t>(static_cast<int8_t>(-5)));      // speed
    buf.writeByte(75);                                                 // direction

    auto msg = UpdateTransformMessage::parse(buf);
    CHECK(msg.objectId == 281479009480346ULL);
    CHECK(msg.x == doctest::Approx(10.0f));
    CHECK(msg.y == doctest::Approx(-5.0f));
    CHECK(msg.z == doctest::Approx(2.0f));
    CHECK(msg.movementCounter == 42);
    CHECK(msg.speed == -5);
    CHECK(msg.direction == 75);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("UpdateTransformWithParentMessage::parse - parentId precedes objectId, /8 scale") {
    soe::PacketBuffer buf;
    buf.writeUint64(281479009480349ULL); // parentId (cell)
    buf.writeUint64(281479009480346ULL); // objectId
    buf.writeUint16(static_cast<uint16_t>(static_cast<int16_t>(40)));  // x: 40/8 = 5.0
    buf.writeUint16(static_cast<uint16_t>(static_cast<int16_t>(-16))); // y (height): -16/8 = -2.0
    buf.writeUint16(static_cast<uint16_t>(static_cast<int16_t>(8)));   // z: 8/8 = 1.0
    buf.writeUint32(7);                                                // movementCounter
    buf.writeByte(0);                                                  // speed
    buf.writeByte(50);                                                 // direction

    auto msg = UpdateTransformWithParentMessage::parse(buf);
    CHECK(msg.parentId == 281479009480349ULL);
    CHECK(msg.objectId == 281479009480346ULL);
    CHECK(msg.x == doctest::Approx(5.0f));
    CHECK(msg.y == doctest::Approx(-2.0f));
    CHECK(msg.z == doctest::Approx(1.0f));
    CHECK(msg.movementCounter == 7);
    CHECK(msg.speed == 0);
    CHECK(msg.direction == 50);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("UpdateTransformMessage::parse - real payload (Kalda Ulzo, Finalizer, Tatooine)") {
    // Captured 2026-07-15 via a temporary hex-dump patch during a live
    // movement-observation run. 20 bytes, independently re-verified
    // field-by-field before being pinned - zero leftover bytes, matches the
    // live console output exactly. Field assignment corrected 2026-07-19
    // (see UpdateTransformMessage.h's own comment): the wire's 2nd float
    // (5.0) is the real height - a plausible value for open Tatooine
    // terrain, unlike the 3rd float (-5038.5), which is genuinely the
    // other horizontal coordinate. This real sample is itself part of the
    // evidence that led to the fix, not just updated to match it.
    auto buf =
        bufferFromHex("e4 33 69 f0 00 00 01 00 fd 32 14 00 46 b1 bf 07 00 00 01 03");

    auto msg = UpdateTransformMessage::parse(buf);
    CHECK(msg.objectId == 281479010137060ULL);
    CHECK(msg.x == doctest::Approx(3263.25f));
    CHECK(msg.y == doctest::Approx(5.0f));
    CHECK(msg.z == doctest::Approx(-5038.5f));
    CHECK(msg.movementCounter == 1983);
    CHECK(msg.speed == 1);
    CHECK(msg.direction == 3);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("UpdateTransformWithParentMessage::parse - real payload (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15, same session as above. 28 bytes, independently
    // re-verified field-by-field - zero leftover bytes. Field assignment
    // corrected 2026-07-19 - see UpdateTransformMessage.h's own comment.
    auto buf = bufferFromHex(
        "fd 85 10 00 00 00 00 00 af c5 72 ef 00 00 01 00 72 00 f9 ff 0b 00 86 fd 14 00 00 59");

    auto msg = UpdateTransformWithParentMessage::parse(buf);
    CHECK(msg.parentId == 1082877ULL);
    CHECK(msg.objectId == 281478993986991ULL);
    CHECK(msg.x == doctest::Approx(14.25f));
    CHECK(msg.y == doctest::Approx(-0.875f));
    CHECK(msg.z == doctest::Approx(1.375f));
    CHECK(msg.movementCounter == 1375622);
    CHECK(msg.speed == 0);
    CHECK(msg.direction == 89);
    CHECK(buf.remaining() == 0);
}
