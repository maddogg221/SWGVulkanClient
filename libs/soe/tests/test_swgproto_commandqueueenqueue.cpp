// Permanent regression test for CommandQueueEnqueue - the first outbound
// game-command message this project builds. Live-verified against a REAL
// Core3 server (Naritus, 2026-07-17): sending `/teleportTo Jinaka` actually
// moved the target character. Before this shape was correct, the server
// logged "Invalid enqueueCommand call" with garbage field values that traced
// byte-for-byte back to an extra 4-byte "unused" field this project's
// builder incorrectly copied from the server->client envelope (which does
// have a trailing unused int32 - the client->server one does not). This test
// pins the corrected wire shape by parsing the builder's own output back
// field-by-field, so a regression to the old (broken) 4-byte-shifted layout
// fails loudly instead of silently breaking live command traffic again.
#include <doctest/doctest.h>

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"
#include "swgproto/CommandQueueEnqueue.h"

using namespace swgproto;

TEST_CASE("buildCommandQueueEnqueue - wire shape matches live-verified Core3 server expectations") {
    const uint64_t objectId = 0x1234567890ABCDEFULL;
    const uint32_t actionCount = 1;
    const uint64_t targetId = 0;
    const std::u16string arguments = u"Jinaka";

    // Deliberately mixed-case, mirroring the real CLI invocation that
    // originally surfaced the missing-lowercase bug - the builder must
    // lowercase this itself since Core3 registers commands lowercase.
    auto bytes = buildCommandQueueEnqueue(objectId, actionCount, "teleportTo", targetId, arguments);

    soe::PacketBuffer buf(bytes.data(), bytes.size());

    CHECK(buf.readUint16() == 0x05); // opCount
    CHECK(buf.readUint32() == soe::MessageHash::compute("ObjControllerMessage"));
    CHECK(buf.readUint32() == 0x0B);  // header1
    CHECK(buf.readUint32() == 0x116); // header2 (CommandQueueEnqueue)
    CHECK(buf.readUint64() == objectId);
    // No trailing "unused" int32 here - the client->server envelope is 4
    // bytes shorter than the server->client one. This is the exact field
    // whose presence caused the live-traffic bug this test guards against.

    uint32_t expectedRemainingSize =
        4 /* actionCount */ + 4 /* actionCRC */ + 8 /* targetId */ +
        4 /* arguments length prefix */ + static_cast<uint32_t>(arguments.size()) * 2;
    CHECK(buf.readUint32() == expectedRemainingSize); // size
    CHECK(buf.readUint32() == actionCount);
    CHECK(buf.readUint32() == soe::MessageHash::compute("teleportto")); // lowercased
    CHECK(buf.readUint64() == targetId);
    CHECK(buf.readUnicode() == arguments);
    CHECK(buf.remaining() == 0);
}
