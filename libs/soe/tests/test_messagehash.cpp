#include <doctest/doctest.h>

#include "soe/MessageHash.h"

using soe::MessageHash;

// Known-good values verified directly from Core3's engine3 source (see
// DISCOVERY.txt) - these are the actual opcodes Core3 computes for these
// message names, not arbitrary test data.
TEST_CASE("MessageHash: matches verified Core3 hashCode values") {
    CHECK(MessageHash::compute("LoginClientId") == 0x41131F96);
    CHECK(MessageHash::compute("CmdSceneReady") == 0x43FD1C22);
}

TEST_CASE("MessageHash: compile-time evaluation via the SOE_MESSAGE_HASH macro") {
    constexpr uint32_t hash = SOE_MESSAGE_HASH("LoginClientId");
    static_assert(hash == 0x41131F96, "compile-time hash must match the runtime value");
    CHECK(hash == 0x41131F96);
}

TEST_CASE("MessageHash: empty string hashes to the seed's complement") {
    CHECK(MessageHash::compute("") == (~0xFFFFFFFFu));
}
