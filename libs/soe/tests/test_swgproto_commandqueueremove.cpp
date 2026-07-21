// Tests for CommandQueueRemove (Phase 4's sixth ObjControllerMessage
// sub-type) - signals a queued command/action completing, firing once per
// attack/ability in real combat traffic alongside ShowFlyText's hit-location
// feedback. The synthetic case pins the field order (uint32 + float + 2x
// uint32, 16 bytes total). The 4 real-byte fixtures come from BOTH servers
// used in this project's Phase 4 combat captures - Finalizer and the user's
// own unmodified-source Proxmox Core3 server - matching the pattern already
// established for ShowFlyText's tests (proving the wire format generalizes
// across Core3-family servers). All independently re-verified via
// PowerShell's [BitConverter]::ToUInt32/ToSingle before being pinned - zero
// leftover bytes on every one. See CommandQueueRemove.h for the
// actionCount field's documented (but not fully understood) bit structure.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/CommandQueueRemove.h"

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

TEST_CASE("CommandQueueRemove::parse - field order") {
    soe::PacketBuffer buf;
    buf.writeUint32(1088);   // actionCount
    buf.writeFloat(1.75f);   // timer
    buf.writeUint32(3);      // tab1
    buf.writeUint32(7);      // tab2

    auto msg = CommandQueueRemove::parse(buf);
    CHECK(msg.actionCount == 1088);
    CHECK(msg.timer == doctest::Approx(1.75f));
    CHECK(msg.tab1 == 3);
    CHECK(msg.tab2 == 7);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("CommandQueueRemove::parse - real payload #1 (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15 during Phase 4 step 3's combat capture. 16 bytes,
    // zero leftover.
    auto buf = bufferFromHex("40 04 00 40 1f 1a dd 3f 00 00 00 00 00 00 00 00");

    auto msg = CommandQueueRemove::parse(buf);
    CHECK(msg.actionCount == 1073742912u);
    CHECK(msg.timer == doctest::Approx(1.72735965f));
    CHECK(msg.tab1 == 0);
    CHECK(msg.tab2 == 0);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("CommandQueueRemove::parse - real payload #2 (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15, same session as #1 above - a later action in the
    // same kill sequence, actionCount incremented by 64 (2x the usual
    // per-instance step of 32 seen across this dataset). 16 bytes, zero
    // leftover.
    auto buf = bufferFromHex("80 04 00 40 97 d3 25 40 00 00 00 00 00 00 00 00");

    auto msg = CommandQueueRemove::parse(buf);
    CHECK(msg.actionCount == 1073742976u);
    CHECK(msg.timer == doctest::Approx(2.59103942f));
    CHECK(msg.tab1 == 0);
    CHECK(msg.tab2 == 0);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("CommandQueueRemove::parse - real payload #3, timer=0 (Proxmox Core3, "
          "unmodified public source)") {
    // Captured 2026-07-15 during Phase 4 step 5/6's Proxmox comparison test
    // (a private test server) - independent confirmation the wire format holds on a
    // second, unmodified-source server. Also the smallest actionCount seen
    // (no high bit set, unlike the Finalizer samples above) and a zero
    // timer. 16 bytes, zero leftover.
    auto buf = bufferFromHex("20 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

    auto msg = CommandQueueRemove::parse(buf);
    CHECK(msg.actionCount == 544u);
    CHECK(msg.timer == doctest::Approx(0.0f));
    CHECK(msg.tab1 == 0);
    CHECK(msg.tab2 == 0);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("CommandQueueRemove::parse - real payload #4, nonzero tab1 (Proxmox Core3)") {
    // Captured 2026-07-15, same session as #3 above - the only real sample
    // seen anywhere in this project with a nonzero tab1/tab2 field,
    // confirming those fields really do carry data sometimes (not always
    // padding). 16 bytes, zero leftover.
    auto buf = bufferFromHex("00 03 00 40 00 00 00 00 03 00 00 00 00 00 00 00");

    auto msg = CommandQueueRemove::parse(buf);
    CHECK(msg.actionCount == 1073742592u);
    CHECK(msg.timer == doctest::Approx(0.0f));
    CHECK(msg.tab1 == 3);
    CHECK(msg.tab2 == 0);
    CHECK(buf.remaining() == 0);
}
