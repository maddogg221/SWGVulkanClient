// Permanent tests for the schema-driven baseline/delta decode engine
// (swgproto/FieldKind.h, ObjectSchema.h, SchemaEngine.h) in isolation, using
// a small synthetic type - NOT CreatureObject/PlayerObject, which get their
// own real-byte fixtures once they're migrated onto this engine (see
// PLAN.md's schema-engine plan, stages 2-5).
#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/SchemaEngine.h"

using namespace swgproto;

namespace {

// A tiny synthetic "ancestor" type - mirrors how TangibleObjectBaseline3/
// IntangibleObjectBaseline3 have no ancestor of their own.
struct TestAncestor {
    int32_t a = 0;   // index 0x00
    float b = 0.0f;  // index 0x01
};

constexpr FieldDescriptor kTestAncestorFields[] = {
    field<FieldKind::Int32, &TestAncestor::a>(0x00, "a"),
    field<FieldKind::Float, &TestAncestor::b>(0x01, "b"),
};
constexpr ObjectSchema kTestAncestorSchema{nullptr, 0, kTestAncestorFields};

// A tiny synthetic "derived" type composing the ancestor - mirrors
// CreatureObjectBaseline3 HAS-A TangibleObjectBaseline3.
struct TestDerived {
    TestAncestor ancestor;
    uint8_t c = 0;   // index 0x02
    std::string d;   // index 0x03
};

constexpr FieldDescriptor kTestDerivedOwnFields[] = {
    field<FieldKind::Byte, &TestDerived::c>(0x02, "c"),
    field<FieldKind::Ascii, &TestDerived::d>(0x03, "d"),
};
constexpr ObjectSchema kTestDerivedSchema{&kTestAncestorSchema, offsetof(TestDerived, ancestor),
                                           kTestDerivedOwnFields};

} // namespace

TEST_CASE("SchemaEngine: decodeBaseline walks ancestor then own fields in order") {
    soe::PacketBuffer buf;
    buf.writeUint32(42);       // a
    buf.writeFloat(3.5f);      // b
    buf.writeByte(7);          // c
    buf.writeUint16(3);
    buf.writeBytes(reinterpret_cast<const uint8_t*>("xyz"), 3); // d

    auto result = decodeBaseline<TestDerived>(kTestDerivedSchema, buf);
    REQUIRE(result.ok());
    const auto& v = result.value();
    CHECK(v.ancestor.a == 42);
    CHECK(v.ancestor.b == doctest::Approx(3.5f));
    CHECK(v.c == 7);
    CHECK(v.d == "xyz");
    CHECK(buf.remaining() == 0);
}

TEST_CASE("SchemaEngine: decodeBaseline reports the failing field on underflow") {
    soe::PacketBuffer buf;
    buf.writeUint32(42); // a only - missing b, c, d

    auto result = decodeBaseline<TestDerived>(kTestDerivedSchema, buf);
    CHECK_FALSE(result.ok());
    CHECK(result.error().find("b:") == 0); // the ancestor's second field should be named in the error
}

TEST_CASE("SchemaEngine: decodeDeltaMessage finds fields in both own and ancestor tables") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x02); // c (own)
    buf.writeByte(9);
    buf.writeUint16(0x00); // a (ancestor)
    buf.writeUint32(100);

    auto result = decodeDeltaMessage(kTestDerivedSchema, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.updates.size() == 2);
    CHECK(result.updates[0].fieldName == "c");
    CHECK(result.updates[0].valueText == "9");
    CHECK(result.updates[1].fieldName == "a");
    CHECK(result.updates[1].valueText == "100");
}

TEST_CASE("SchemaEngine: decodeDeltaMessage stops at an unmapped field index") {
    soe::PacketBuffer buf;
    buf.writeUint16(0x02); // c (own) - known
    buf.writeByte(1);
    buf.writeUint16(0xFF); // unmapped anywhere in the chain
    buf.writeUint32(0);

    auto result = decodeDeltaMessage(kTestDerivedSchema, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.updates.size() == 1);
    CHECK(result.updates[0].fieldName == "c");
}

TEST_CASE("SchemaEngine: applyDeltaMessage writes into a real struct's own AND ancestor fields") {
    TestDerived instance;
    instance.ancestor.a = 1;
    instance.ancestor.b = 1.5f;
    instance.c = 1;
    instance.d = "unchanged";

    soe::PacketBuffer buf;
    buf.writeUint16(0x02); // c (own)
    buf.writeByte(9);
    buf.writeUint16(0x00); // a (ancestor) - the whole point: needs the offset findDeltaField alone doesn't track
    buf.writeUint32(100);

    auto result = applyDeltaMessage(kTestDerivedSchema, &instance, 2, buf);
    CHECK_FALSE(result.stoppedEarly);
    REQUIRE(result.appliedFieldIndices.size() == 2);
    CHECK(result.appliedFieldIndices[0] == 0x02);
    CHECK(result.appliedFieldIndices[1] == 0x00);

    // Touched fields changed...
    CHECK(instance.c == 9);
    CHECK(instance.ancestor.a == 100);
    // ...and untouched fields didn't - proves this is a targeted write, not
    // a full re-decode that happens to leave old bytes alone.
    CHECK(instance.ancestor.b == doctest::Approx(1.5f));
    CHECK(instance.d == "unchanged");
    CHECK(buf.remaining() == 0);
}

TEST_CASE("SchemaEngine: applyDeltaMessage stops at an unmapped index, prior writes stick") {
    TestDerived instance;
    instance.c = 1;

    soe::PacketBuffer buf;
    buf.writeUint16(0x02); // c (own) - known, applied
    buf.writeByte(42);
    buf.writeUint16(0xFF); // unmapped anywhere in the chain
    buf.writeUint32(0);

    auto result = applyDeltaMessage(kTestDerivedSchema, &instance, 2, buf);
    CHECK(result.stoppedEarly);
    REQUIRE(result.appliedFieldIndices.size() == 1);
    CHECK(result.appliedFieldIndices[0] == 0x02);
    // The one field before the bad index really was written, matching
    // decodeDeltaMessage's existing partial-progress semantics.
    CHECK(instance.c == 42);
}
