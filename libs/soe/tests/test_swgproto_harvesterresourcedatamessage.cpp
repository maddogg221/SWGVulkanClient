// Test for HarvesterResourceDataMessage (header2=0xEA), the reply to the
// real "harvesterGetResourceData" QueueCommand. Traced and hand-decoded
// live during Phase 9's harvester activation investigation (zero leftover
// bytes across 3 real captures - see DISCOVERY.txt/KNOWN_UNKNOWNS.md) but
// never turned into an actual swgproto type at the time. The raw bytes
// from that session weren't preserved verbatim, so this fixture is
// RECONSTRUCTED from the real field VALUES that session documented (not
// fabricated): harvesterObjectId 562949970712690 (the real moisture
// vaporator placed that session), two real active "water_vapor_naboo"
// resource spawns seen nearby - "Ejoga" (5629499551006506) and "Oci"
// (5629499551006214) - and a real density value (18.4%, i.e. densityPercent
// 18) documented as "the key diagnostic that confirmed spawn density was
// never the [harvester activation] blocker". The second entry's density
// (22%) is not independently documented and is a plausible fill-in, not a
// captured value.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/HarvesterResourceDataMessage.h"

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

TEST_CASE("HarvesterResourceDataMessage::parse - reconstructed from real documented values") {
    auto buf = bufferFromHex(
        "72 d8 07 01 00 00 02 00 "
        "02 00 00 00 "
        "2a 3f 00 01 00 00 14 00 "
        "05 00 45 6a 6f 67 61 "
        "11 00 77 61 74 65 72 5f 76 61 70 6f 72 5f 6e 61 62 6f 6f "
        "12 "
        "06 3e 00 01 00 00 14 00 "
        "03 00 4f 63 69 "
        "11 00 77 61 74 65 72 5f 76 61 70 6f 72 5f 6e 61 62 6f 6f "
        "16");

    auto msg = HarvesterResourceDataMessage::parse(buf);
    CHECK(msg.harvesterObjectId == 562949970712690ULL);
    REQUIRE(msg.resources.size() == 2);

    CHECK(msg.resources[0].resourceSpawnId == 5629499551006506ULL);
    CHECK(msg.resources[0].name == "Ejoga");
    CHECK(msg.resources[0].resourceType == "water_vapor_naboo");
    CHECK(msg.resources[0].densityPercent == 18);

    CHECK(msg.resources[1].resourceSpawnId == 5629499551006214ULL);
    CHECK(msg.resources[1].name == "Oci");
    CHECK(msg.resources[1].resourceType == "water_vapor_naboo");
    CHECK(msg.resources[1].densityPercent == 22);

    CHECK(buf.remaining() == 0);
}
