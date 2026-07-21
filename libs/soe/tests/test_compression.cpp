#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "soe/Compression.h"

using soe::Compression;

TEST_CASE("Compression: decompress(compress(x)) restores the original bytes") {
    std::string text =
        "The quick brown bang bang bang bang bang bang bang bang bang fox jumps over the lazy dog.";
    std::vector<uint8_t> input(text.begin(), text.end());

    auto compressed = Compression::compress(input.data(), input.size());
    auto decompressed = Compression::decompress(compressed.data(), compressed.size(), input.size() + 64);

    REQUIRE(decompressed.size() == input.size());
    CHECK(decompressed == input);
}

TEST_CASE("Compression: highly repetitive data compresses smaller than the input") {
    std::vector<uint8_t> input(500, 0x41); // 500 repeated 'A' bytes

    auto compressed = Compression::compress(input.data(), input.size());

    CHECK(compressed.size() < input.size());
}

TEST_CASE("Compression: round trip on empty input") {
    std::vector<uint8_t> input;

    auto compressed = Compression::compress(input.data(), input.size());
    auto decompressed = Compression::decompress(compressed.data(), compressed.size(), 16);

    CHECK(decompressed.empty());
}
