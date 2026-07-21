// Permanent regression test for GuildObject BASE3/6 (Phase 6, the second
// "low hanging fruit" item alongside CellObject). Both fixtures below are
// REAL captured bytes (1 confirmed sighting each this session).
//
// NOT a per-guild object, despite the name and despite each entry looking
// like "your guild's info" - GuildObjectImplementation::sendBaselinesTo()
// is dead code (a no-op) in source. All real GILD traffic comes from
// GuildManagerImplementation, a zone-wide singleton service sent to every
// player at login/scene-reset, using the manager's own object ID
// (confirmed live: objectId=4017234474, far smaller than any normal
// per-instance SceneObject ID). What this decodes is a single, server-
// wide directory of every guild that exists - the real BASE3 fixture
// below has 101 real guild entries ("<guildID>:<guildAbbrev>", e.g.
// "1001866848:Feds"), not one player's own guild data.
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/GuildObjectBaseline3.h"
#include "swgproto/GuildObjectBaseline6.h"

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

TEST_CASE("GuildObjectBaseline3::parse - real payload (server-wide guild directory)") {
    auto buf = bufferFromHex(
        "00 00 80 3f 0f 00 53 74 72 69 6e 67 5f 69 64 5f "
        "74 61 62 6c 65 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 65 00 00 00 00 00 00 00 0f 00 31 30 30 "
        "31 38 36 36 38 34 38 3a 46 65 64 73 10 00 31 30 "
        "31 35 34 33 31 30 36 37 3a 2d 53 55 4e 2d 10 00 "
        "31 30 32 37 32 36 39 30 37 34 3a 46 43 4c 55 42 "
        "0e 00 31 30 37 34 35 32 32 34 3a 52 4b 41 44 45 "
        "0f 00 31 31 33 35 39 39 38 38 36 39 3a 44 47 45 "
        "4e 0e 00 31 31 34 33 32 37 31 30 3a 56 65 6e 6f "
        "6d 0e 00 31 31 35 33 39 34 37 31 36 37 3a 41 54 "
        "4b 0d 00 31 32 35 36 30 39 33 30 32 3a 52 45 44 "
        "0e 00 31 33 38 30 36 39 36 33 38 37 3a 53 74 4b "
        "0f 00 31 34 31 35 38 39 36 38 35 36 3a 45 43 48 "
        "4f 0c 00 31 34 32 32 38 35 37 35 39 3a 43 5a 0f "
        "00 31 34 35 33 34 31 36 39 39 34 3a 45 57 4f 4b "
        "0f 00 31 34 37 31 34 39 30 34 37 33 3a 49 52 4f "
        "4e 0e 00 31 35 31 39 34 36 35 34 36 39 3a 53 49 "
        "58 10 00 31 35 33 38 32 32 32 39 35 37 3a 54 52 "
        "41 43 4b 0f 00 31 35 35 35 36 39 35 31 34 36 3a "
        "50 41 4c 53 0f 00 31 35 35 36 38 36 34 31 38 33 "
        "3a 47 45 45 4b 0e 00 31 35 35 39 33 39 32 31 32 "
        "30 3a 52 42 4e 0c 00 31 36 38 32 37 33 36 37 35 "
        "3a 41 45 0f 00 31 36 39 38 37 35 34 36 37 3a 45 "
        "6c 64 65 72 10 00 31 37 35 35 34 30 30 39 35 34 "
        "3a 52 45 42 45 4c 0e 00 31 37 36 38 36 38 33 36 "
        "30 32 3a 4a 48 43 0e 00 31 37 39 31 37 36 38 37 "
        "38 30 3a 52 53 46 0e 00 31 38 32 35 38 37 34 39 "
        "33 38 3a 49 63 65 0e 00 31 38 33 38 32 34 36 36 "
        "32 38 3a 49 53 52 0f 00 31 38 35 33 32 32 38 35 "
        "33 32 3a 48 4f 50 45 10 00 31 38 35 36 37 36 34 "
        "39 38 34 3a 47 68 6f 73 74 10 00 31 38 36 39 32 "
        "31 36 36 33 33 3a 2d 4b 6f 47 2d 0f 00 31 38 39 "
        "38 33 32 31 30 37 34 3a 57 4f 4f 4b 10 00 31 39 "
        "30 32 38 37 39 39 32 33 3a 54 6f 6f 6c 73 10 00 "
        "32 30 30 32 34 38 34 32 32 35 3a 54 52 41 44 45 "
        "0e 00 32 30 31 31 39 36 36 34 35 38 3a 4f 58 59 "
        "0e 00 32 30 34 37 37 33 31 37 34 3a 53 49 54 48 "
        "0d 00 32 30 35 38 34 36 32 34 37 34 3a 44 4c 0f "
        "00 32 30 35 39 34 31 35 37 34 38 3a 54 4b 44 53 "
        "0e 00 32 31 34 31 32 33 39 36 33 36 3a 41 54 47 "
        "0f 00 32 31 36 37 30 31 33 35 32 31 3a 4f 4e 59 "
        "58 0e 00 32 31 39 31 30 33 37 38 38 36 3a 54 57 "
        "4c 0e 00 32 32 31 37 37 30 33 33 39 3a 54 53 59 "
        "4e 0e 00 32 32 33 33 32 34 35 39 34 39 3a 41 43 "
        "41 0f 00 32 32 36 33 35 38 35 32 35 31 3a 53 6c "
        "54 48 10 00 32 32 38 39 39 37 33 37 31 34 3a 49 "
        "54 52 50 44 10 00 32 33 34 32 36 38 30 37 36 39 "
        "3a 47 75 69 64 65 0e 00 32 33 34 32 37 37 36 33 "
        "32 37 3a 4f 46 54 0e 00 32 34 31 36 35 35 35 35 "
        "39 39 3a 48 4e 52 0d 00 32 34 33 38 33 32 32 32 "
        "36 35 3a 47 47 0e 00 32 34 33 39 38 34 37 38 35 "
        "3a 4e 69 6b 61 0e 00 32 34 39 33 38 34 36 31 33 "
        "34 3a 48 52 46 0d 00 32 34 39 35 30 33 38 33 39 "
        "3a 4e 47 53 0e 00 32 34 39 35 33 34 33 30 39 38 "
        "3a 52 4e 43 0e 00 32 35 32 39 31 33 33 35 37 34 "
        "3a 54 41 47 0c 00 32 37 32 31 35 39 34 31 36 34 "
        "3a 43 0e 00 32 37 35 33 30 39 37 32 38 35 3a 52 "
        "47 53 0f 00 32 37 35 37 31 33 30 30 31 31 3a 53 "
        "49 4e 52 0e 00 32 37 39 35 37 34 31 30 35 3a 52 "
        "6f 53 45 10 00 32 38 31 33 38 31 38 37 30 37 3a "
        "47 75 69 6c 64 0f 00 32 38 33 39 38 32 36 30 38 "
        "32 3a 4c 58 56 49 0e 00 32 38 34 32 32 32 32 35 "
        "37 35 3a 69 4d 62 0d 00 32 39 35 38 38 30 34 37 "
        "32 39 3a 45 56 0e 00 33 30 31 36 38 36 31 34 31 "
        "30 3a 42 44 43 0e 00 33 30 32 39 35 38 35 30 32 "
        "32 3a 41 6f 57 0e 00 33 30 33 35 36 35 35 34 33 "
        "35 3a 49 4e 51 0c 00 33 30 34 35 30 32 35 32 37 "
        "37 3a 49 0f 00 33 30 36 36 33 36 31 37 34 30 3a "
        "66 69 72 65 0f 00 33 32 38 38 39 33 31 31 38 35 "
        "3a 2d 56 56 2d 0e 00 33 32 38 39 31 34 32 39 32 "
        "33 3a 53 55 4e 0c 00 33 34 31 33 38 31 37 39 33 "
        "32 3a 2d 0f 00 33 34 31 36 33 34 38 32 32 36 3a "
        "4a 69 56 45 0f 00 33 34 38 35 38 36 30 33 35 36 "
        "3a 53 49 4f 4e 0f 00 33 34 39 32 36 34 36 32 30 "
        "35 3a 2d 42 48 2d 0e 00 33 34 39 33 35 34 31 36 "
        "31 32 3a 53 64 57 0c 00 33 35 33 35 36 37 31 35 "
        "32 3a 46 56 0e 00 33 36 36 30 39 33 31 39 35 3a "
        "44 4f 4f 4d 0f 00 33 36 39 39 31 39 37 30 31 3a "
        "53 48 44 41 4e 0f 00 33 37 31 32 34 30 32 34 34 "
        "31 3a 54 54 54 54 0f 00 33 38 31 31 33 37 37 34 "
        "36 34 3a 48 55 54 54 10 00 33 38 35 35 37 38 31 "
        "32 37 39 3a 53 52 44 69 76 10 00 33 38 37 39 30 "
        "33 31 37 31 30 3a 43 4f 42 52 41 0f 00 33 39 35 "
        "38 35 37 35 31 31 39 3a 46 49 53 54 10 00 33 39 "
        "39 34 30 30 34 34 35 32 3a 4a 41 57 41 53 10 00 "
        "34 30 34 39 36 38 36 34 38 37 3a 53 50 49 43 45 "
        "0e 00 34 31 33 32 33 30 31 30 39 38 3a 43 49 4e "
        "0f 00 34 31 37 32 31 32 33 39 35 35 3a 52 45 47 "
        "53 0f 00 34 31 37 35 39 32 37 39 32 38 3a 5a 65 "
        "72 6f 10 00 34 31 39 33 32 34 30 30 32 33 3a 58 "
        "2d 4d 45 4e 0d 00 34 32 32 31 30 31 37 36 36 3a "
        "50 56 45 10 00 34 32 36 39 30 38 37 32 39 30 3a "
        "4b 52 41 59 54 10 00 34 32 37 30 31 37 35 39 37 "
        "30 3a 53 48 2d 52 55 10 00 34 32 37 33 30 37 31 "
        "32 38 33 3a 4a 75 44 47 45 0c 00 34 34 39 37 39 "
        "37 30 33 37 3a 56 46 0d 00 34 38 35 32 39 37 35 "
        "37 33 3a 49 43 45 0d 00 35 30 34 33 34 34 33 31 "
        "37 3a 53 59 4e 0f 00 35 32 30 39 33 33 31 31 33 "
        "3a 41 6c 64 75 72 0e 00 36 32 38 32 31 30 32 33 "
        "35 3a 54 55 53 4b 0e 00 36 37 39 33 33 34 39 38 "
        "35 3a 4d 41 54 53 0d 00 37 32 35 33 37 32 32 32 "
        "39 3a 4e 52 4e 0c 00 37 36 37 33 31 39 31 36 34 "
        "3a 42 48 0d 00 38 30 38 37 38 32 31 39 34 3a 49 "
        "4e 46 0d 00 38 32 38 30 31 38 38 31 31 3a 42 54 "
        "46 0d 00 39 35 30 37 31 33 35 35 36 3a 4e 47 52 "
        "0f 00 39 36 39 39 31 34 35 31 31 3a 52 45 49 47 "
        "4e ");

    auto result = GuildObjectBaseline3::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.complexity == doctest::Approx(1.0f));
    CHECK(b.objectName.file == "String_id_table");
    CHECK(b.objectName.stringId == "");
    CHECK(b.name.empty());
    CHECK(b.unknownField == 0);

    REQUIRE(b.guildList.size() == 101);
    CHECK(b.guildList.front() == "1001866848:Feds");
    CHECK(b.guildList.back() == "969914511:REIGN");
    CHECK(b.guildList[5] == "11432710:Venom");

    CHECK(buf.remaining() == 0);
}

TEST_CASE("GuildObjectBaseline6::parse - real payload") {
    auto buf = bufferFromHex("3b 00 00 00");

    auto result = GuildObjectBaseline6::parse(buf);
    REQUIRE(result.ok());
    const auto& b = result.value();

    CHECK(b.unknownConst == 0x3B);

    CHECK(buf.remaining() == 0);
}
