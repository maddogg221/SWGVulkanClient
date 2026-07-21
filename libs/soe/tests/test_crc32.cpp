#include <vector>

#include <doctest/doctest.h>

#include "soe/Crc32.h"
#include "soe/PacketBuffer.h"

using soe::PacketBuffer;
using soe::PacketCrc;

TEST_CASE("PacketCrc: append then test round-trips true") {
    PacketBuffer buf;
    buf.writeUint16(0x0900);
    buf.writeUint16BE(1); // sequence
    buf.writeAscii("hello");
    buf.writeByte(0x00); // compression flag
    buf.writeUint16(0);  // CRC placeholder

    PacketCrc::append(buf, 0xCAFEBABE);

    CHECK(PacketCrc::test(buf, 0xCAFEBABE));
}

TEST_CASE("PacketCrc: a different seed fails verification") {
    PacketBuffer buf;
    buf.writeUint16(0x0900);
    buf.writeUint16BE(1);
    buf.writeAscii("hello");
    buf.writeByte(0x00);
    buf.writeUint16(0);

    PacketCrc::append(buf, 0xCAFEBABE);

    CHECK_FALSE(PacketCrc::test(buf, 0x11111111));
}

TEST_CASE("PacketCrc: tampering with the payload fails verification") {
    PacketBuffer buf;
    buf.writeUint16(0x0900);
    buf.writeUint16BE(1);
    buf.writeAscii("hello");
    buf.writeByte(0x00);
    buf.writeUint16(0);

    PacketCrc::append(buf, 0xCAFEBABE);
    REQUIRE(PacketCrc::test(buf, 0xCAFEBABE));

    // Flip a bit in the payload after the CRC was computed.
    buf.writeByteAt(4, buf.peekByte(4) ^ 0x01);

    CHECK_FALSE(PacketCrc::test(buf, 0xCAFEBABE));
}

TEST_CASE("PacketCrc: generate is deterministic for the same input and seed") {
    std::vector<uint8_t> data = {0x09, 0x00, 0x00, 0x01, 'h', 'i'};
    uint32_t a = PacketCrc::generate(data.data(), data.size(), 12345);
    uint32_t b = PacketCrc::generate(data.data(), data.size(), 12345);
    CHECK(a == b);

    uint32_t c = PacketCrc::generate(data.data(), data.size(), 54321);
    CHECK(a != c);
}
