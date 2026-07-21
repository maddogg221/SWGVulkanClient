// Tests for swgproto::ObjectStateDispatcher (Phase 5 step 4) - mirrors
// test_objcontrollerdispatcher.cpp's coverage one level further down: this
// class routes BaselinesMessage/DeltasMessage traffic by a (object type
// FourCC, baseline number) key pair instead of a single integer. Built
// standalone with no behavior change to any existing tool yet - see
// PHASE_05_STATUS.md.
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjectStateDispatcher.h"

using soe::PacketBuffer;
using swgproto::BaselineEnvelope;
using swgproto::ObjectStateDispatcher;

namespace {

// "CREO"/"PLAY"/"TANO" as their raw wire uint32 FourCC value - objectType
// is read via a plain readUint32() and interpreted big-endian byte-by-byte
// by objectTypeTag() (see BaselineEnvelope.cpp), so the encoding here must
// match that exactly.
uint32_t fourCC(const std::string& tag) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(tag[0])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(tag[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(tag[2])) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(tag[3]));
}

// Builds a buffer positioned at the start of BaselineEnvelope's own fields -
// i.e. exactly what dispatchBaseline()/dispatchDelta() expect, matching
// what a soe::MessageDispatcher handler already has after consuming
// opCount+hash. `size` defaults to a valid minimum (2, per
// BaselineEnvelope::parse()'s own validation) unless overridden to build a
// deliberately-malformed envelope.
PacketBuffer buildEnvelope(uint64_t objectId, const std::string& objectType,
                            uint8_t baselineNumber, uint16_t count, uint32_t size = 2,
                            const std::string& extraField = "") {
    PacketBuffer buf;
    buf.writeUint64(objectId);
    buf.writeUint32(fourCC(objectType));
    buf.writeByte(baselineNumber);
    buf.writeUint32(size);
    buf.writeUint16(count);
    if (!extraField.empty()) {
        buf.writeAscii(extraField);
    }
    return buf;
}

} // namespace

TEST_CASE("ObjectStateDispatcher: dispatchBaseline routes to the matching (type, baselineNumber) "
          "handler") {
    ObjectStateDispatcher dispatcher;
    bool called = false;
    uint64_t receivedObjectId = 0;
    std::string receivedField;

    dispatcher.onBaseline("CREO", 3, [&](const BaselineEnvelope& envelope, PacketBuffer& buf) {
        called = true;
        receivedObjectId = envelope.objectId;
        receivedField = buf.readAscii();
    });

    auto buf = buildEnvelope(281478530303830ULL, "CREO", 3, 0, 2, "hello");
    dispatcher.dispatchBaseline(buf);

    CHECK(called);
    CHECK(receivedObjectId == 281478530303830ULL);
    CHECK(receivedField == "hello");
}

TEST_CASE("ObjectStateDispatcher: handler receives the full envelope, not just the dispatch key") {
    ObjectStateDispatcher dispatcher;
    uint32_t receivedSize = 0;
    uint16_t receivedCount = 0;

    dispatcher.onBaseline("PLAY", 8, [&](const BaselineEnvelope& envelope, PacketBuffer&) {
        receivedSize = envelope.size;
        receivedCount = envelope.count;
    });

    auto buf = buildEnvelope(1ULL, "PLAY", 8, 42, 99);
    dispatcher.dispatchBaseline(buf);

    CHECK(receivedSize == 99);
    CHECK(receivedCount == 42);
}

TEST_CASE("ObjectStateDispatcher: different baseline numbers for the same type are independent") {
    ObjectStateDispatcher dispatcher;
    int base3Calls = 0;
    int base6Calls = 0;

    dispatcher.onBaseline("PLAY", 3, [&](const BaselineEnvelope&, PacketBuffer&) { ++base3Calls; });
    dispatcher.onBaseline("PLAY", 6, [&](const BaselineEnvelope&, PacketBuffer&) { ++base6Calls; });

    auto buf3 = buildEnvelope(1ULL, "PLAY", 3, 0);
    dispatcher.dispatchBaseline(buf3);
    auto buf6 = buildEnvelope(1ULL, "PLAY", 6, 0);
    dispatcher.dispatchBaseline(buf6);

    CHECK(base3Calls == 1);
    CHECK(base6Calls == 1);
}

TEST_CASE("ObjectStateDispatcher: falls back to the unknown-baseline handler for an "
          "unregistered key") {
    ObjectStateDispatcher dispatcher;
    bool unknownCalled = false;
    std::string unknownType;
    uint8_t unknownBaselineNumber = 0;

    dispatcher.onUnknownBaseline([&](const BaselineEnvelope& envelope, PacketBuffer&) {
        unknownCalled = true;
        unknownType = swgproto::objectTypeTag(envelope.objectType);
        unknownBaselineNumber = envelope.baselineNumber;
    });

    auto buf = buildEnvelope(1ULL, "TANO", 3, 0);
    dispatcher.dispatchBaseline(buf);

    CHECK(unknownCalled);
    CHECK(unknownType == "TANO");
    CHECK(unknownBaselineNumber == 3);
}

TEST_CASE("ObjectStateDispatcher: default unknown-baseline handling is a silent no-op") {
    ObjectStateDispatcher dispatcher;

    std::ostringstream captured;
    std::streambuf* oldBuf = std::cout.rdbuf(captured.rdbuf());
    auto buf = buildEnvelope(1ULL, "TANO", 3, 0);
    CHECK_NOTHROW(dispatcher.dispatchBaseline(buf));
    std::cout.rdbuf(oldBuf);

    CHECK(captured.str().empty());
}

TEST_CASE("ObjectStateDispatcher: a malformed envelope (size < 2) is dropped, no handler called") {
    ObjectStateDispatcher dispatcher;
    bool handlerCalled = false;
    bool unknownCalled = false;

    dispatcher.onBaseline("CREO", 3,
                           [&](const BaselineEnvelope&, PacketBuffer&) { handlerCalled = true; });
    dispatcher.onUnknownBaseline(
        [&](const BaselineEnvelope&, PacketBuffer&) { unknownCalled = true; });

    auto buf = buildEnvelope(1ULL, "CREO", 3, 0, /*size=*/1); // invalid: size must be >= 2
    CHECK_NOTHROW(dispatcher.dispatchBaseline(buf));

    CHECK_FALSE(handlerCalled);
    CHECK_FALSE(unknownCalled);
}

TEST_CASE("ObjectStateDispatcher: a handler that throws is isolated, not propagated") {
    ObjectStateDispatcher dispatcher;
    dispatcher.onBaseline("CREO", 1, [](const BaselineEnvelope&, PacketBuffer&) {
        throw std::runtime_error("boom");
    });

    auto buf = buildEnvelope(1ULL, "CREO", 1, 0);
    CHECK_NOTHROW(dispatcher.dispatchBaseline(buf));
}

TEST_CASE("ObjectStateDispatcher: offBaseline() removes a registered handler, falling back to "
          "unknown") {
    ObjectStateDispatcher dispatcher;
    bool handlerCalled = false;
    bool unknownCalled = false;

    dispatcher.onBaseline("CREO", 3,
                           [&](const BaselineEnvelope&, PacketBuffer&) { handlerCalled = true; });
    dispatcher.onUnknownBaseline(
        [&](const BaselineEnvelope&, PacketBuffer&) { unknownCalled = true; });

    dispatcher.offBaseline("CREO", 3);
    auto buf = buildEnvelope(1ULL, "CREO", 3, 0);
    dispatcher.dispatchBaseline(buf);

    CHECK_FALSE(handlerCalled);
    CHECK(unknownCalled);
}

TEST_CASE("ObjectStateDispatcher: baseline and delta registries are fully independent") {
    // Same (type, baselineNumber) key, deliberately different handlers for
    // each - this is the real-world case (e.g. CREO baseline 3 parses a
    // full struct; CREO delta 3 decodes update entries against a schema).
    ObjectStateDispatcher dispatcher;
    bool baselineCalled = false;
    bool deltaCalled = false;

    dispatcher.onBaseline("CREO", 3,
                           [&](const BaselineEnvelope&, PacketBuffer&) { baselineCalled = true; });
    dispatcher.onDelta("CREO", 3, [&](const BaselineEnvelope&, PacketBuffer&) { deltaCalled = true; });

    auto baselineBuf = buildEnvelope(1ULL, "CREO", 3, 0);
    dispatcher.dispatchBaseline(baselineBuf);
    CHECK(baselineCalled);
    CHECK_FALSE(deltaCalled);

    auto deltaBuf = buildEnvelope(1ULL, "CREO", 3, 0);
    dispatcher.dispatchDelta(deltaBuf);
    CHECK(deltaCalled);
}

TEST_CASE("ObjectStateDispatcher: offUnknownBaseline/offUnknownDelta reset back to the silent "
          "no-op default") {
    // Regression guard for a real dangling-reference risk: an
    // ObjectStateDispatcher instance that outlives the scope which
    // registered a capturing onUnknownBaseline/onUnknownDelta handler has
    // no other way to avoid invoking that handler again later against
    // now-destroyed captured state - offBaseline()/offDelta() only cover
    // per-key handlers, not these.
    ObjectStateDispatcher dispatcher;
    bool called = false;
    dispatcher.onUnknownBaseline([&](const BaselineEnvelope&, PacketBuffer&) { called = true; });
    dispatcher.onUnknownDelta([&](const BaselineEnvelope&, PacketBuffer&) { called = true; });

    dispatcher.offUnknownBaseline();
    dispatcher.offUnknownDelta();

    auto buf1 = buildEnvelope(1ULL, "TANO", 3, 0);
    CHECK_NOTHROW(dispatcher.dispatchBaseline(buf1));
    auto buf2 = buildEnvelope(1ULL, "TANO", 3, 0);
    CHECK_NOTHROW(dispatcher.dispatchDelta(buf2));

    CHECK_FALSE(called);
}

TEST_CASE("ObjectStateDispatcher: dispatchDelta falls back to its own unknown-delta handler, "
          "independent of onUnknownBaseline") {
    ObjectStateDispatcher dispatcher;
    bool unknownBaselineCalled = false;
    bool unknownDeltaCalled = false;

    dispatcher.onUnknownBaseline(
        [&](const BaselineEnvelope&, PacketBuffer&) { unknownBaselineCalled = true; });
    dispatcher.onUnknownDelta(
        [&](const BaselineEnvelope&, PacketBuffer&) { unknownDeltaCalled = true; });

    auto buf = buildEnvelope(1ULL, "TANO", 3, 0);
    dispatcher.dispatchDelta(buf);

    CHECK_FALSE(unknownBaselineCalled);
    CHECK(unknownDeltaCalled);
}
