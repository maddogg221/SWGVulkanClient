// Test for StartingLocationListMessage (Phase 4 step 12's
// ObjControllerMessage sub-type, header2=0x1FC). SYNTHETIC ONLY - fires
// from a real StartingLocationTerminal object's right-click radial menu
// selection (StartingLocationTerminalImplementation::handleObjectMenuSelect,
// selectedID==20), which needs this project's still-undecoded SUI/
// radial-menu protocol to reach live - same situation as CommandQueueAdd.
// Unlike ObjectMenuResponse (deferred entirely, see KNOWN_UNKNOWNS.md),
// this message's shape has zero ambiguity from source:
// StartingLocationList.h's insertToMessage() is a plain insertInt(count) +
// count * StartingLocation entries (each a fixed field sequence, no offset
// patching or recursion).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/StartingLocationListMessage.h"

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

TEST_CASE("StartingLocationListMessage::parse - synthetic payload, two entries") {
    // count=2, then two StartingLocationEntry records:
    // entry 1: location="Theed", planet="naboo", x=10.0, y=20.0, cell="",
    //   image="theed", description="Capital city", flag=1
    // entry 2: location="Coronet", planet="corellia", x=0.0, y=0.0,
    //   cell="", image="coronet", description="", flag=1
    auto buf = bufferFromHex(
        "02 00 00 00 "
        "05 00 54 68 65 65 64 "
        "05 00 6e 61 62 6f 6f "
        "00 00 20 41 "
        "00 00 a0 41 "
        "00 00 "
        "05 00 74 68 65 65 64 "
        "0c 00 43 61 70 69 74 61 6c 20 63 69 74 79 "
        "01 "
        "07 00 43 6f 72 6f 6e 65 74 "
        "08 00 63 6f 72 65 6c 6c 69 61 "
        "00 00 00 00 "
        "00 00 00 00 "
        "00 00 "
        "07 00 63 6f 72 6f 6e 65 74 "
        "00 00 "
        "01");

    auto msg = StartingLocationListMessage::parse(buf);
    REQUIRE(msg.locations.size() == 2);

    CHECK(msg.locations[0].location == "Theed");
    CHECK(msg.locations[0].planet == "naboo");
    CHECK(msg.locations[0].x == doctest::Approx(10.0f));
    CHECK(msg.locations[0].y == doctest::Approx(20.0f));
    CHECK(msg.locations[0].cell == "");
    CHECK(msg.locations[0].image == "theed");
    CHECK(msg.locations[0].description == "Capital city");
    CHECK(msg.locations[0].unknownFlag == 1);

    CHECK(msg.locations[1].location == "Coronet");
    CHECK(msg.locations[1].planet == "corellia");
    CHECK(msg.locations[1].x == doctest::Approx(0.0f));
    CHECK(msg.locations[1].y == doctest::Approx(0.0f));
    CHECK(msg.locations[1].cell == "");
    CHECK(msg.locations[1].image == "coronet");
    CHECK(msg.locations[1].description == "");
    CHECK(msg.locations[1].unknownFlag == 1);

    CHECK(buf.remaining() == 0);
}
