#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include "soe/FragmentReassembler.h"
#include "soe/PacketBuffer.h"

using soe::FragmentReassembler;
using soe::FragmentStatus;
using soe::PacketBuffer;

namespace {

// Builds a fragment's payload buffer (as FragmentReassembler::addFragment()
// expects to receive it - positioned right after the fragment's own 4-byte
// opcode+sequence header, which SoeSession::receiveMessages() strips before
// handing off). If `firstFragmentTotalSize` is set, prefixes the 4-byte
// big-endian total-size field a real first fragment carries.
PacketBuffer buildFragmentPayload(const std::vector<uint8_t>& data,
                                   std::optional<uint32_t> firstFragmentTotalSize = std::nullopt) {
    PacketBuffer buf;
    if (firstFragmentTotalSize) {
        buf.writeUint32BE(*firstFragmentTotalSize);
    }
    buf.writeBytes(data);
    return buf;
}

} // namespace

TEST_CASE("FragmentReassembler: a single fragment that already covers the whole message completes immediately") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};

    auto buf = buildFragmentPayload(payload, static_cast<uint32_t>(payload.size()));
    auto result = reassembler.addFragment(100, buf);

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == payload);
}

TEST_CASE("FragmentReassembler: reassembles a message split across two in-order fragments") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<uint8_t> firstChunk(full.begin(), full.begin() + 6);
    std::vector<uint8_t> secondChunk(full.begin() + 6, full.end());

    auto first = buildFragmentPayload(firstChunk, static_cast<uint32_t>(full.size()));
    auto firstResult = reassembler.addFragment(100, first);
    CHECK(firstResult.status == FragmentStatus::Incomplete);

    auto second = buildFragmentPayload(secondChunk);
    auto secondResult = reassembler.addFragment(101, second);

    CHECK(secondResult.status == FragmentStatus::Complete);
    CHECK(secondResult.payload == full);
}

TEST_CASE("FragmentReassembler: reassembles a message split across three in-order fragments") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'};

    auto first = buildFragmentPayload({full.begin(), full.begin() + 3},
                                       static_cast<uint32_t>(full.size()));
    CHECK(reassembler.addFragment(500, first).status == FragmentStatus::Incomplete);

    auto second = buildFragmentPayload({full.begin() + 3, full.begin() + 6});
    CHECK(reassembler.addFragment(501, second).status == FragmentStatus::Incomplete);

    auto third = buildFragmentPayload({full.begin() + 6, full.end()});
    auto result = reassembler.addFragment(502, third);

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == full);
}

TEST_CASE("FragmentReassembler: sequence numbers wrap around at 16 bits") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {1, 2, 3, 4};

    auto first = buildFragmentPayload({full.begin(), full.begin() + 2},
                                       static_cast<uint32_t>(full.size()));
    CHECK(reassembler.addFragment(0xFFFF, first).status == FragmentStatus::Incomplete);

    auto second = buildFragmentPayload({full.begin() + 2, full.end()});
    auto result = reassembler.addFragment(0x0000, second); // wrapped

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == full);
}

TEST_CASE("FragmentReassembler: a fragment offering more bytes than needed is clipped to the total size") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {1, 2, 3, 4};
    // Total size says 4, but this "first fragment" hands over 6 bytes -
    // matches Core3's own addFragment() clipping behavior rather than
    // treating it as malformed.
    std::vector<uint8_t> overstuffed = {1, 2, 3, 4, 0xAA, 0xBB};

    auto frag = buildFragmentPayload(overstuffed, static_cast<uint32_t>(full.size()));
    auto result = reassembler.addFragment(100, frag);

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == full);
}

TEST_CASE("FragmentReassembler: a completed reassembly resets state for the next one") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> firstMessage = {1, 2, 3};
    std::vector<uint8_t> secondMessage = {9, 9};

    auto frag1 = buildFragmentPayload(firstMessage, static_cast<uint32_t>(firstMessage.size()));
    CHECK(reassembler.addFragment(100, frag1).status == FragmentStatus::Complete);

    // A fresh reassembly's first fragment can carry any sequence number -
    // it's the new baseline, not required to continue the previous one.
    auto frag2 = buildFragmentPayload(secondMessage, static_cast<uint32_t>(secondMessage.size()));
    auto result = reassembler.addFragment(9000, frag2);

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == secondMessage);
}

TEST_CASE("FragmentReassembler: a total size of zero is rejected without throwing") {
    FragmentReassembler reassembler;
    auto frag = buildFragmentPayload({}, 0);
    auto result = reassembler.addFragment(100, frag);

    CHECK(result.status == FragmentStatus::Invalid);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("FragmentReassembler: a total size over the sanity cap is rejected without throwing") {
    FragmentReassembler reassembler;
    auto frag = buildFragmentPayload({}, 2000000); // > MAX_COMPLETE_FRAG_SIZE (1,500,000)
    auto result = reassembler.addFragment(100, frag);

    CHECK(result.status == FragmentStatus::Invalid);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("FragmentReassembler: a first fragment too short for its total-size field is rejected without throwing") {
    FragmentReassembler reassembler;
    PacketBuffer tooShort;
    tooShort.writeByte(0x01); // only 1 byte - can't hold a 4-byte total-size field
    auto result = reassembler.addFragment(100, tooShort);

    CHECK(result.status == FragmentStatus::Invalid);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("FragmentReassembler: rejecting a first fragment leaves the reassembler usable afterward") {
    FragmentReassembler reassembler;
    auto badFrag = buildFragmentPayload({}, 0);
    CHECK(reassembler.addFragment(100, badFrag).status == FragmentStatus::Invalid);

    std::vector<uint8_t> message = {5, 6, 7};
    auto goodFrag = buildFragmentPayload(message, static_cast<uint32_t>(message.size()));
    auto result = reassembler.addFragment(200, goodFrag);

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == message);
}

TEST_CASE("FragmentReassembler: a fragment that skips a sequence number is rejected as out of order") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {1, 2, 3, 4, 5, 6};

    auto first = buildFragmentPayload({full.begin(), full.begin() + 3},
                                       static_cast<uint32_t>(full.size()));
    CHECK(reassembler.addFragment(100, first).status == FragmentStatus::Incomplete);

    // Expected sequence 101, but this one claims 105 - a gap, not a
    // legitimate continuation.
    auto skipped = buildFragmentPayload({full.begin() + 3, full.end()});
    auto result = reassembler.addFragment(105, skipped);

    CHECK(result.status == FragmentStatus::Invalid);
    CHECK_FALSE(result.error.empty());
}

TEST_CASE("FragmentReassembler: a duplicate/retransmitted fragment is rejected as out of order") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {1, 2, 3, 4, 5, 6};

    auto first = buildFragmentPayload({full.begin(), full.begin() + 3},
                                       static_cast<uint32_t>(full.size()));
    CHECK(reassembler.addFragment(100, first).status == FragmentStatus::Incomplete);

    // Same sequence number seen again instead of the next one.
    auto duplicate = buildFragmentPayload({full.begin() + 3, full.end()});
    auto result = reassembler.addFragment(100, duplicate);

    CHECK(result.status == FragmentStatus::Invalid);
}

TEST_CASE("FragmentReassembler: an out-of-order fragment resets state so the next reassembly still works") {
    FragmentReassembler reassembler;
    std::vector<uint8_t> full = {1, 2, 3, 4};

    auto first = buildFragmentPayload({full.begin(), full.begin() + 2},
                                       static_cast<uint32_t>(full.size()));
    CHECK(reassembler.addFragment(100, first).status == FragmentStatus::Incomplete);

    auto outOfOrder = buildFragmentPayload({full.begin() + 2, full.end()});
    CHECK(reassembler.addFragment(999, outOfOrder).status == FragmentStatus::Invalid);

    std::vector<uint8_t> nextMessage = {7, 8, 9};
    auto freshFrag = buildFragmentPayload(nextMessage, static_cast<uint32_t>(nextMessage.size()));
    auto result = reassembler.addFragment(1, freshFrag);

    CHECK(result.status == FragmentStatus::Complete);
    CHECK(result.payload == nextMessage);
}
