// Permanent tests for assets::IffReader - the generic FORM/chunk container
// reader shared by every SWG client asset format this project parses
// (.iff/.apt/.msh). Uses a small hand-built synthetic buffer (mirrors
// test_schema_engine.cpp's own synthetic-fixture style) rather than a real
// asset file, since this test's job is proving the CONTAINER format parses
// correctly in isolation - each concrete format (SharedObjectTemplate,
// AppearanceTemplate, StaticMesh) gets its own real-byte fixture tests once
// implemented.
#include <doctest/doctest.h>

#include <stdexcept>

#include "assets/IffReader.h"

using namespace assets;

namespace {

// FORM/'TEST' containing one leaf DATA chunk (4-byte payload). Tag/size
// fields are big-endian (confirmed against engine3's IffStream/Form, which
// use readNetInt()/htonl() for both) - hand-encoded here rather than
// written via any helper, so this test doesn't depend on IffReader's own
// correctness to build its own fixture.
const std::vector<uint8_t> kSyntheticBuffer = {
    0x46, 0x4F, 0x52, 0x4D, // 'FORM'
    0x00, 0x00, 0x00, 0x10, // size = 16
    0x54, 0x45, 0x53, 0x54, // formType = 'TEST'
    0x44, 0x41, 0x54, 0x41, // child id = 'DATA'
    0x00, 0x00, 0x00, 0x04, // child size = 4
    0x01, 0x02, 0x03, 0x04, // child payload
};

} // namespace

TEST_CASE("IffReader: parses a synthetic FORM containing one leaf chunk") {
    auto topLevel = IffReader::parse(kSyntheticBuffer);
    REQUIRE(topLevel.size() == 1);

    const IffChunk& form = topLevel[0];
    CHECK(form.id == kFormTag);
    CHECK(form.formType == 0x54455354u); // 'TEST'
    REQUIRE(form.children.size() == 1);

    IffChunk child = form.children[0]; // copied (not const&) so its own read cursor can advance
    CHECK(child.id == 0x44415441u); // 'DATA'
    CHECK(child.id != kFormTag);
    CHECK(child.children.empty());
    REQUIRE(child.data.remaining() == 4);
    CHECK(child.data.readByte() == 0x01);
    CHECK(child.data.readByte() == 0x02);
    CHECK(child.data.readByte() == 0x03);
    CHECK(child.data.readByte() == 0x04);
}

TEST_CASE("IffReader: findChild locates a direct child by id, nullptr if absent") {
    auto topLevel = IffReader::parse(kSyntheticBuffer);
    const IffChunk& form = topLevel[0];

    const IffChunk* found = form.findChild(0x44415441u); // 'DATA'
    REQUIRE(found != nullptr);
    CHECK(found->id == 0x44415441u);

    CHECK(form.findChild(0x58585858u) == nullptr); // 'XXXX' - not present
}

TEST_CASE("IffReader: a truncated buffer throws rather than silently misparsing") {
    std::vector<uint8_t> truncated(kSyntheticBuffer.begin(), kSyntheticBuffer.begin() + 10);
    CHECK_THROWS_AS(IffReader::parse(truncated), std::out_of_range);
}
