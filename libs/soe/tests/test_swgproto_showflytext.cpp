// Tests for ShowFlyText (Phase 4's fifth ObjControllerMessage sub-type), the
// message Core3-family servers actually use for real combat hit-location
// feedback (see DISCOVERY.txt's "PHASE 4 STEP 3"/"PHASE 4 STEP 5" -
// CombatAction/CombatSpam are dead code on both Finalizer AND unmodified
// public Core3 source). The synthetic case pins the field order (in
// particular the uint32 spacer sitting between the two ASCII strings, easy
// to miss). The real-byte fixtures below come from TWO independent servers -
// Finalizer (character "Kalda Ulzo", Phase 4 step 3) and the user's own
// Proxmox-hosted unmodified Core3 server (Phase 4 step 5's anti-bot-theory
// comparison test) - deliberately kept from both to prove the wire format
// generalizes across Core3-family servers, not just one. All independently
// re-verified field-by-field (targetObjectId cross-checked via PowerShell's
// [BitConverter]::ToUInt64, matching the ObjControllerMessage envelope's own
// objectId exactly, as expected since Core3's constructor inserts
// creo->getObjectID() into both places) before being pinned - zero leftover
// bytes on every one, despite differing string lengths (43-52 bytes).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/ShowFlyText.h"

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

TEST_CASE("ShowFlyText::parse - field order, including the spacer between strings") {
    soe::PacketBuffer buf;
    buf.writeUint64(281479010288372ULL); // targetObjectId
    buf.writeAscii("combat_effects");    // file
    buf.writeUint32(0);                  // spacer
    buf.writeAscii("hit_body");          // entry
    buf.writeFloat(1.0f);                // scale
    buf.writeByte(10);                   // red
    buf.writeByte(20);                   // green
    buf.writeByte(30);                   // blue
    buf.writeByte(5);                    // flags

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281479010288372ULL);
    CHECK(msg.file == "combat_effects");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "hit_body");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 10);
    CHECK(msg.green == 20);
    CHECK(msg.blue == 30);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("ShowFlyText::parse - real payload, npc_reaction/flytext/alert (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15 during Phase 4 step 3's combat capture. 49 bytes,
    // zero leftover.
    auto buf = bufferFromHex(
        "0f c5 72 ef 00 00 01 00 14 00 6e 70 63 5f 72 65 61 63 74 69 6f 6e 2f 66 6c 79 74 65 "
        "78 74 00 00 00 00 05 00 61 6c 65 72 74 00 00 80 3f ff 00 00 05");

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281478993986831ULL);
    CHECK(msg.file == "npc_reaction/flytext");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "alert");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 255);
    CHECK(msg.green == 0);
    CHECK(msg.blue == 0);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("ShowFlyText::parse - real payload, combat_effects/hit_head (Kalda Ulzo, Finalizer)") {
    // Captured 2026-07-15 during the mob-kill combat capture - real
    // hit-location feedback. 46 bytes, zero leftover.
    auto buf = bufferFromHex(
        "f4 82 6b f0 00 00 01 00 0e 00 63 6f 6d 62 61 74 5f 65 66 66 65 63 74 73 00 00 00 00 "
        "08 00 68 69 74 5f 68 65 61 64 00 00 80 3f 00 00 ff 05");

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281479010288372ULL);
    CHECK(msg.file == "combat_effects");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "hit_head");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 0);
    CHECK(msg.green == 0);
    CHECK(msg.blue == 255);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("ShowFlyText::parse - real payload, combat_effects/go_intimidated (Kalda Ulzo, "
          "Finalizer)") {
    // Captured 2026-07-15, same session/target as the hit_head fixture
    // above (same targetObjectId - the same mob's intimidation status
    // effect firing moments earlier in the kill sequence). 52 bytes, zero
    // leftover.
    auto buf = bufferFromHex(
        "f4 82 6b f0 00 00 01 00 0e 00 63 6f 6d 62 61 74 5f 65 66 66 65 63 74 73 00 00 00 00 "
        "0e 00 67 6f 5f 69 6e 74 69 6d 69 64 61 74 65 64 00 00 80 3f 00 ff 00 05");

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281479010288372ULL);
    CHECK(msg.file == "combat_effects");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "go_intimidated");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 0);
    CHECK(msg.green == 255);
    CHECK(msg.blue == 0);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("ShowFlyText::parse - real payload, combat_effects/block (Proxmox Core3, "
          "unmodified public source)") {
    // Captured 2026-07-15 during Phase 4 step 5's anti-bot-theory comparison
    // test against the user's own Proxmox-hosted Core3 server
    // (a private test server) - independent confirmation the wire format holds on a
    // second, unmodified-source server, not just Finalizer. 43 bytes, zero
    // leftover.
    auto buf = bufferFromHex(
        "d6 5d 04 01 00 00 01 00 0e 00 63 6f 6d 62 61 74 5f 65 66 66 65 63 74 73 00 00 00 00 "
        "05 00 62 6c 6f 63 6b 00 00 80 3f 00 ff 00 05");

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281474993774038ULL);
    CHECK(msg.file == "combat_effects");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "block");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 0);
    CHECK(msg.green == 255);
    CHECK(msg.blue == 0);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("ShowFlyText::parse - real payload, combat_effects/hit_lleg (Proxmox Core3)") {
    // Captured 2026-07-15, same session as the block fixture above - a hit
    // location (left leg) not seen in any Finalizer capture, confirming
    // this server's combat coverage is at least as rich (all 6 limbs vs.
    // Finalizer's partial set). 46 bytes, zero leftover.
    auto buf = bufferFromHex(
        "28 5f 04 01 00 00 01 00 0e 00 63 6f 6d 62 61 74 5f 65 66 66 65 63 74 73 00 00 00 00 "
        "08 00 68 69 74 5f 6c 6c 65 67 00 00 80 3f 00 ff 00 05");

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281474993774376ULL);
    CHECK(msg.file == "combat_effects");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "hit_lleg");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 0);
    CHECK(msg.green == 255);
    CHECK(msg.blue == 0);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}

TEST_CASE("ShowFlyText::parse - real payload, combat_effects/warcry_hit (Proxmox Core3)") {
    // Captured 2026-07-15, same session/target as the hit_lleg fixture above
    // - a special-ability hit type not seen on Finalizer at all. 48 bytes,
    // zero leftover.
    auto buf = bufferFromHex(
        "28 5f 04 01 00 00 01 00 0e 00 63 6f 6d 62 61 74 5f 65 66 66 65 63 74 73 00 00 00 00 "
        "0a 00 77 61 72 63 72 79 5f 68 69 74 00 00 80 3f 00 ff 00 05");

    auto msg = ShowFlyText::parse(buf);
    CHECK(msg.targetObjectId == 281474993774376ULL);
    CHECK(msg.file == "combat_effects");
    CHECK(msg.spacer == 0);
    CHECK(msg.entry == "warcry_hit");
    CHECK(msg.scale == doctest::Approx(1.0f));
    CHECK(msg.red == 0);
    CHECK(msg.green == 255);
    CHECK(msg.blue == 0);
    CHECK(msg.flags == 5);
    CHECK(buf.remaining() == 0);
}
