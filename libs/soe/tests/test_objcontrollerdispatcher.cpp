// Tests for swgproto::ObjControllerDispatcher (Phase 5 step 1) - mirrors
// test_messagedispatcher.cpp's coverage one level down: this class routes
// ObjControllerMessage's own header2 field instead of a top-level message
// hash. Built standalone with no behavior change to any existing tool yet -
// see PHASE_05_STATUS.md.
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjControllerDispatcher.h"
#include "swgproto/ShowFlyText.h"

using soe::PacketBuffer;
using swgproto::ObjControllerDispatcher;
using swgproto::ObjControllerMessage;

namespace {

// Builds a buffer positioned at the start of ObjControllerMessage's own
// fields - i.e. exactly what dispatch() expects, matching what a
// soe::MessageDispatcher handler already has after consuming opCount+hash.
PacketBuffer buildEnvelope(uint32_t header1, uint32_t header2, uint64_t objectId,
                            const std::string& extraField = "") {
    PacketBuffer buf;
    buf.writeUint32(header1);
    buf.writeUint32(header2);
    buf.writeUint64(objectId);
    buf.writeUint32(0); // unused
    if (!extraField.empty()) {
        buf.writeAscii(extraField);
    }
    return buf;
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(hex);
    std::string token;
    while (iss >> token) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
    }
    return bytes;
}

} // namespace

TEST_CASE("ObjControllerDispatcher: dispatches to the matching registered handler") {
    ObjControllerDispatcher dispatcher;
    bool called = false;
    uint64_t receivedObjectId = 0;
    std::string receivedField;

    dispatcher.on(0x1BD, [&](const ObjControllerMessage& envelope, PacketBuffer& buf) {
        called = true;
        receivedObjectId = envelope.objectId;
        receivedField = buf.readAscii();
    });

    auto buf = buildEnvelope(0x1B, 0x1BD, 281479010288372ULL, "hello");
    dispatcher.dispatch(buf);

    CHECK(called);
    CHECK(receivedObjectId == 281479010288372ULL);
    CHECK(receivedField == "hello");
}

TEST_CASE("ObjControllerDispatcher: handler receives header1/objectId correctly, not just header2") {
    // Regression guard for exactly the kind of field-transposition bug this
    // class exists to prevent - the whole envelope must be parsed
    // correctly before handler lookup, not just the dispatch key.
    ObjControllerDispatcher dispatcher;
    uint32_t receivedHeader1 = 0;
    uint64_t receivedObjectId = 0;

    dispatcher.on(0x1BD, [&](const ObjControllerMessage& envelope, PacketBuffer&) {
        receivedHeader1 = envelope.header1;
        receivedObjectId = envelope.objectId;
    });

    auto buf = buildEnvelope(0x1B, 0x1BD, 281479010288372ULL);
    dispatcher.dispatch(buf);

    CHECK(receivedHeader1 == 0x1B);
    CHECK(receivedObjectId == 281479010288372ULL);
}

TEST_CASE("ObjControllerDispatcher: falls back to the unknown handler for an unregistered header2") {
    ObjControllerDispatcher dispatcher;
    bool unknownCalled = false;
    uint32_t unknownHeader2 = 0;

    dispatcher.onUnknown([&](const ObjControllerMessage& envelope, PacketBuffer&) {
        unknownCalled = true;
        unknownHeader2 = envelope.header2;
    });

    auto buf = buildEnvelope(0x1B, 0xCC, 12345ULL);
    dispatcher.dispatch(buf);

    CHECK(unknownCalled);
    CHECK(unknownHeader2 == 0xCC);
}

TEST_CASE("ObjControllerDispatcher: default unknown handling is a silent no-op") {
    // Deliberately different from soe::MessageDispatcher's default (which
    // logs each distinct hash once) - see onUnknown()'s header comment for
    // why silence is the right default here.
    ObjControllerDispatcher dispatcher;

    std::ostringstream captured;
    std::streambuf* oldBuf = std::cout.rdbuf(captured.rdbuf());
    auto buf = buildEnvelope(0x1B, 0xCC, 12345ULL);
    CHECK_NOTHROW(dispatcher.dispatch(buf));
    std::cout.rdbuf(oldBuf);

    CHECK(captured.str().empty());
}

TEST_CASE("ObjControllerDispatcher: a handler that throws is isolated, not propagated") {
    ObjControllerDispatcher dispatcher;
    dispatcher.on(0x1, [](const ObjControllerMessage&, PacketBuffer&) {
        throw std::runtime_error("boom");
    });

    auto buf = buildEnvelope(0x1B, 0x1, 1ULL);
    CHECK_NOTHROW(dispatcher.dispatch(buf));
}

TEST_CASE("ObjControllerDispatcher: a handler that throws does not stop later dispatch calls") {
    ObjControllerDispatcher dispatcher;
    int goodCalls = 0;
    dispatcher.on(0x1, [](const ObjControllerMessage&, PacketBuffer&) {
        throw std::runtime_error("boom");
    });
    dispatcher.on(0x2, [&](const ObjControllerMessage&, PacketBuffer&) { ++goodCalls; });

    auto buf1 = buildEnvelope(0x1B, 0x1, 1ULL);
    dispatcher.dispatch(buf1);
    auto buf2 = buildEnvelope(0x1B, 0x2, 1ULL);
    dispatcher.dispatch(buf2);

    CHECK(goodCalls == 1);
}

TEST_CASE("ObjControllerDispatcher: re-registering a header2 replaces the previous handler") {
    ObjControllerDispatcher dispatcher;
    int firstCalls = 0;
    int secondCalls = 0;

    dispatcher.on(0x1, [&](const ObjControllerMessage&, PacketBuffer&) { ++firstCalls; });
    dispatcher.on(0x1, [&](const ObjControllerMessage&, PacketBuffer&) { ++secondCalls; });

    auto buf = buildEnvelope(0x1B, 0x1, 1ULL);
    dispatcher.dispatch(buf);

    CHECK(firstCalls == 0);
    CHECK(secondCalls == 1);
}

TEST_CASE("ObjControllerDispatcher: off() removes a registered handler, falling back to unknown") {
    ObjControllerDispatcher dispatcher;
    bool handlerCalled = false;
    bool unknownCalled = false;

    dispatcher.on(0x1, [&](const ObjControllerMessage&, PacketBuffer&) { handlerCalled = true; });
    dispatcher.onUnknown(
        [&](const ObjControllerMessage&, PacketBuffer&) { unknownCalled = true; });

    dispatcher.off(0x1);
    auto buf = buildEnvelope(0x1B, 0x1, 1ULL);
    dispatcher.dispatch(buf);

    CHECK_FALSE(handlerCalled);
    CHECK(unknownCalled);
}

TEST_CASE("ObjControllerDispatcher: off() on a header2 with no registered handler is a no-op") {
    ObjControllerDispatcher dispatcher;
    CHECK_NOTHROW(dispatcher.off(0x1));
}

TEST_CASE("ObjControllerDispatcher: real ShowFlyText payload decodes end-to-end through the "
          "dispatcher") {
    // Integration check with real production data (the same Finalizer
    // fixture pinned in test_swgproto_showflytext.cpp, which is itself only
    // ShowFlyText's OWN fields - the envelope was already stripped before
    // that fixture was captured) - proves the dispatcher's envelope
    // parsing + handoff works with a real multi-field sub-type, not just a
    // synthetic single ASCII field. Prepends a real envelope (header2 =
    // ShowFlyText's controller type) ahead of the real sub-payload, since
    // dispatch() expects the FULL payload starting at header1, unlike
    // ShowFlyText::parse() itself which starts right after the envelope.
    ObjControllerDispatcher dispatcher;
    std::string decodedEntry;

    dispatcher.on(swgproto::kShowFlyTextControllerType,
                  [&](const ObjControllerMessage&, PacketBuffer& buf) {
                      auto flyText = swgproto::ShowFlyText::parse(buf);
                      decodedEntry = flyText.entry;
                  });

    auto buf = buildEnvelope(0x1B, swgproto::kShowFlyTextControllerType, 281478993986831ULL);
    auto payloadBytes = hexToBytes(
        "0f c5 72 ef 00 00 01 00 14 00 6e 70 63 5f 72 65 61 63 74 69 6f 6e 2f 66 6c 79 74 65 "
        "78 74 00 00 00 00 05 00 61 6c 65 72 74 00 00 80 3f ff 00 00 05");
    buf.writeBytes(payloadBytes.data(), payloadBytes.size());
    dispatcher.dispatch(buf);

    CHECK(decodedEntry == "alert");
    CHECK(buf.remaining() == 0);
}
