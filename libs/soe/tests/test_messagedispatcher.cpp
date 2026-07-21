#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <doctest/doctest.h>

#include "soe/MessageDispatcher.h"
#include "soe/PacketBuffer.h"

using soe::MessageDispatcher;
using soe::PacketBuffer;

namespace {

std::vector<uint8_t> buildPayload(uint16_t opCount, uint32_t hash, const std::string& field = "") {
    PacketBuffer buf;
    buf.writeUint16(opCount);
    buf.writeUint32(hash);
    if (!field.empty()) {
        buf.writeAscii(field);
    }
    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace

TEST_CASE("MessageDispatcher: dispatches to the matching registered handler") {
    MessageDispatcher dispatcher;
    bool called = false;
    std::string received;

    dispatcher.on(0x1234, [&](PacketBuffer& buf) {
        called = true;
        received = buf.readAscii();
    });

    dispatcher.dispatch(buildPayload(1, 0x1234, "hello"));

    CHECK(called);
    CHECK(received == "hello");
}

TEST_CASE("MessageDispatcher: falls back to the unknown handler for an unregistered hash") {
    MessageDispatcher dispatcher;
    bool unknownCalled = false;
    uint32_t unknownHash = 0;

    dispatcher.onUnknown([&](uint32_t hash, PacketBuffer&) {
        unknownCalled = true;
        unknownHash = hash;
    });

    dispatcher.dispatch(buildPayload(1, 0xDEADBEEF));

    CHECK(unknownCalled);
    CHECK(unknownHash == 0xDEADBEEF);
}

TEST_CASE("MessageDispatcher: default unknown handler does not throw") {
    MessageDispatcher dispatcher;
    CHECK_NOTHROW(dispatcher.dispatch(buildPayload(1, 0xABCDEF01)));
}

TEST_CASE("MessageDispatcher: a handler that throws is isolated, not propagated") {
    MessageDispatcher dispatcher;
    dispatcher.on(0x1, [](PacketBuffer&) { throw std::runtime_error("boom"); });

    CHECK_NOTHROW(dispatcher.dispatch(buildPayload(1, 0x1)));
}

TEST_CASE("MessageDispatcher: a handler that throws does not stop later dispatch calls") {
    MessageDispatcher dispatcher;
    int goodCalls = 0;
    dispatcher.on(0x1, [](PacketBuffer&) { throw std::runtime_error("boom"); });
    dispatcher.on(0x2, [&](PacketBuffer&) { ++goodCalls; });

    dispatcher.dispatch(buildPayload(1, 0x1));
    dispatcher.dispatch(buildPayload(1, 0x2));

    CHECK(goodCalls == 1);
}

TEST_CASE("MessageDispatcher: re-registering a hash replaces the previous handler") {
    MessageDispatcher dispatcher;
    int firstCalls = 0;
    int secondCalls = 0;

    dispatcher.on(0x1, [&](PacketBuffer&) { ++firstCalls; });
    dispatcher.on(0x1, [&](PacketBuffer&) { ++secondCalls; });

    dispatcher.dispatch(buildPayload(1, 0x1));

    CHECK(firstCalls == 0);
    CHECK(secondCalls == 1);
}

TEST_CASE("MessageDispatcher: off() removes a registered handler, falling back to unknown") {
    MessageDispatcher dispatcher;
    bool handlerCalled = false;
    bool unknownCalled = false;

    dispatcher.on(0x1, [&](PacketBuffer&) { handlerCalled = true; });
    dispatcher.onUnknown([&](uint32_t, PacketBuffer&) { unknownCalled = true; });

    dispatcher.off(0x1);
    dispatcher.dispatch(buildPayload(1, 0x1));

    CHECK_FALSE(handlerCalled);
    CHECK(unknownCalled);
}

TEST_CASE("MessageDispatcher: off() on a hash with no registered handler is a no-op") {
    MessageDispatcher dispatcher;
    CHECK_NOTHROW(dispatcher.off(0x1));
}

TEST_CASE("MessageDispatcher: default unknown handler logs each distinct hash only once") {
    // Regression guard for real-traffic spam observed in a populated zone
    // during movement-observation testing - the SAME unknown hash can
    // arrive thousands of times in one session, and the default handler
    // used to log every single occurrence.
    MessageDispatcher dispatcher;

    std::ostringstream captured;
    std::streambuf* oldBuf = std::cout.rdbuf(captured.rdbuf());
    dispatcher.dispatch(buildPayload(1, 0xABCDEF01));
    dispatcher.dispatch(buildPayload(1, 0xABCDEF01));
    dispatcher.dispatch(buildPayload(1, 0xABCDEF01));
    std::cout.rdbuf(oldBuf);

    std::string output = captured.str();
    size_t firstOccurrence = output.find("abcdef01");
    REQUIRE(firstOccurrence != std::string::npos);
    CHECK(output.find("abcdef01", firstOccurrence + 1) == std::string::npos);
}

TEST_CASE("MessageDispatcher: a custom onUnknown handler is called every time, not deduped") {
    // The dedup in the test above is specific to the DEFAULT handler - a
    // caller that explicitly overrides onUnknown() asked to see every
    // occurrence, so it must not be silently deduped underneath them.
    MessageDispatcher dispatcher;
    int unknownCalls = 0;
    dispatcher.onUnknown([&](uint32_t, PacketBuffer&) { ++unknownCalls; });

    dispatcher.dispatch(buildPayload(1, 0xDEADBEEF));
    dispatcher.dispatch(buildPayload(1, 0xDEADBEEF));

    CHECK(unknownCalls == 2);
}
