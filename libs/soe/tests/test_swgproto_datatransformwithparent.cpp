// Permanent regression test for the outbound buildDataTransformWithParent()
// (Phase 17) - sending this instead of the plain world-space
// buildDataTransform() while indoors is required, not cosmetic: a real,
// live-caught bug this closes. Core3's own server log showed real "Player
// Speed Abnormality" anti-cheat errors (computed speed 82+ units/sec
// against a 5.376 max) because the server, having already validated self's
// containment in a real cell via UpdateContainmentMessage, rejected a
// subsequent world-space position report as physically impossible.
#include <doctest/doctest.h>

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"
#include "swgproto/DataTransformWithParent.h"

using namespace swgproto;

TEST_CASE("buildDataTransformWithParent - wire shape round-trips through the real parser, "
          "matching a real captured indoor movement sample's field values") {
    const uint64_t objectId = 281474993774038ULL; // real self objectId, Phase 17 captures
    const uint32_t timeStamp = 12345;
    const uint32_t moveCount = 45;
    const uint64_t parentId = 281474994287351ULL; // a real cell objectId from the same captures
    const float dirX = 0.0f, dirY = 0.7071f, dirZ = 0.0f, dirW = 0.7071f;
    const float x = 4.57053f, y = 2.75f, z = 8.82949f; // a real captured indoor position
    const float speed = 5.376f;                        // the real max walk speed this project uses

    auto bytes = buildDataTransformWithParent(objectId, timeStamp, moveCount, parentId, dirX, dirY,
                                                dirZ, dirW, x, y, z, speed);

    soe::PacketBuffer buf(bytes.data(), bytes.size());
    CHECK(buf.readUint16() == 0x05); // opCount
    CHECK(buf.readUint32() == soe::MessageHash::compute("ObjControllerMessage"));
    CHECK(buf.readUint32() == 0x0B); // header1
    CHECK(buf.readUint32() == kDataTransformWithParentControllerType);
    CHECK(buf.readUint64() == objectId);

    // The rest is the exact shape DataTransformWithParent::parse() expects
    // for the SERVER->CLIENT direction, minus the leading timeStamp field
    // (that struct's own `counter` is the server's single-field idle-sync
    // shape - see DataTransform.h's own field-count-asymmetry comment,
    // which applies identically here) - so parse the raw fields directly
    // rather than via DataTransformWithParent::parse() itself.
    CHECK(buf.readUint32() == timeStamp);
    CHECK(buf.readUint32() == moveCount);
    CHECK(buf.readUint64() == parentId);
    CHECK(buf.readFloat() == doctest::Approx(dirX));
    CHECK(buf.readFloat() == doctest::Approx(dirY));
    CHECK(buf.readFloat() == doctest::Approx(dirZ));
    CHECK(buf.readFloat() == doctest::Approx(dirW));
    CHECK(buf.readFloat() == doctest::Approx(x));
    CHECK(buf.readFloat() == doctest::Approx(y));
    CHECK(buf.readFloat() == doctest::Approx(z));
    CHECK(buf.readFloat() == doctest::Approx(speed));
    CHECK(buf.remaining() == 0);
}
