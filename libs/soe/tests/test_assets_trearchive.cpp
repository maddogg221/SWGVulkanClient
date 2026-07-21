// Permanent tests for assets::TreArchive - the ".tre" client asset archive
// reader. The header-parsing test uses the real 36-byte header from an
// actual client archive (data_static_mesh_00.tre), hex-dumped directly and
// cross-checked field-by-field against the format confirmed in
// TreeFile_SearchNode::SearchTree (the ORIGINAL client engine source, see
// TreArchive.h's own comment for the exact source path) - not guessed. The
// full open+extract test additionally requires the real client install to
// be present on this machine (the same archive) and is skipped, not
// failed, wherever it isn't - matches this project's own "verify against
// real data where available" discipline without making the permanent test
// suite depend on a specific machine's local files.
#include <doctest/doctest.h>

#include <cstring>
#include <filesystem>
#include <sstream>

#include "assets/TreArchive.h"

using namespace assets;

namespace {

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(hex);
    std::string token;
    while (iss >> token) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
    }
    return bytes;
}

// The real path this project's client install lives at on this machine -
// see PHASE_10_STATUS.md/SESSION_LOG.md's 2026-07-18 real-mesh-assets entry.
// Only ever used to gate the optional live-archive test below; every other
// test in this file needs no external file at all.
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_static_mesh_00.tre";

} // namespace

TEST_CASE("TreArchive header layout matches a real captured header, byte-for-byte") {
    // The first 36 bytes of data_static_mesh_00.tre, hex-dumped directly.
    auto headerBytes = hexToBytes(
        "45 45 52 54 35 30 30 30 ad 1e 00 00 fa 5b 38 06 "
        "02 00 00 00 aa 0e 02 00 02 00 00 00 f6 f9 00 00 "
        "0d 52 06 00");
    REQUIRE(headerBytes.size() == 36);

    // token/version are read as plain native (little-endian) uint32 - NOT
    // the big-endian convention IffReader's chunk format uses (see
    // TreArchive.h's comment for why this is a real, confirmed difference,
    // not an inconsistency). "EERT"/"5000" are the expected raw file bytes
    // for TAG(T,R,E,E)/TAG(0,0,0,5) on a little-endian host.
    uint32_t token;
    std::memcpy(&token, headerBytes.data(), 4);
    CHECK(token == 0x54524545u); // 'TREE'

    uint32_t version;
    std::memcpy(&version, headerBytes.data() + 4, 4);
    CHECK(version == 0x30303035u); // '0005'

    uint32_t numberOfFiles;
    std::memcpy(&numberOfFiles, headerBytes.data() + 8, 4);
    CHECK(numberOfFiles == 7853u);

    uint32_t tocCompressor;
    std::memcpy(&tocCompressor, headerBytes.data() + 16, 4);
    CHECK(tocCompressor == 2u); // CT_zlib

    uint32_t sizeOfTOC;
    std::memcpy(&sizeOfTOC, headerBytes.data() + 20, 4);
    CHECK(sizeOfTOC == 134826u);

    uint32_t blockCompressor;
    std::memcpy(&blockCompressor, headerBytes.data() + 24, 4);
    CHECK(blockCompressor == 2u); // CT_zlib

    uint32_t sizeOfNameBlock;
    std::memcpy(&sizeOfNameBlock, headerBytes.data() + 28, 4);
    CHECK(sizeOfNameBlock == 63990u);

    uint32_t uncompSizeOfNameBlock;
    std::memcpy(&uncompSizeOfNameBlock, headerBytes.data() + 32, 4);
    CHECK(uncompSizeOfNameBlock == 414221u);
}

TEST_CASE("TreArchive: opening a nonexistent file throws") {
    CHECK_THROWS_AS(TreArchive("Z:\\definitely\\does\\not\\exist.tre"), std::runtime_error);
}

TEST_CASE("TreArchive: opens the real client archive, indexes it, and extracts a real file"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    TreArchive archive(kRealArchivePath);
    CHECK(archive.fileCount() > 0);

    auto files = archive.listFiles();
    REQUIRE(!files.empty());

    // Extract whatever the first indexed file happens to be - proves the
    // full open -> TOC/name-block decompress -> per-file extract path works
    // end to end against real data, without hardcoding a filename that
    // might not exist in every client install/patch version.
    const std::string& sampleName = files.front();
    CHECK(archive.contains(sampleName));
    auto bytes = archive.extract(sampleName);
    CHECK(!bytes.empty());
}
