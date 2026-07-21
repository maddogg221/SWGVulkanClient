#include <stdexcept>

#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"

using soe::PacketBuffer;

TEST_CASE("PacketBuffer: little-endian round trip for scalar types") {
    PacketBuffer buf;
    buf.writeByte(0xAB);
    buf.writeUint16(0x0100);
    buf.writeUint32(0x41131F96);
    buf.writeUint64(0x0123456789ABCDEFULL);
    buf.writeFloat(3.5f);

    CHECK(buf.readByte() == 0xAB);
    CHECK(buf.readUint16() == 0x0100);
    CHECK(buf.readUint32() == 0x41131F96);
    CHECK(buf.readUint64() == 0x0123456789ABCDEFULL);
    CHECK(buf.readFloat() == doctest::Approx(3.5f));
}

TEST_CASE("PacketBuffer: uint16 is written little-endian on the wire") {
    // 0x0100 as a plain (native/little-endian) uint16 write must land as
    // bytes [0x00, 0x01] - matching Core3's own insertShort(0x0100) for
    // SessionRequest on little-endian hardware.
    PacketBuffer buf;
    buf.writeUint16(0x0100);
    REQUIRE(buf.size() == 2);
    CHECK(buf.data()[0] == 0x00);
    CHECK(buf.data()[1] == 0x01);
}

TEST_CASE("PacketBuffer: big-endian helpers differ from little-endian ones") {
    PacketBuffer buf;
    buf.writeUint16BE(0x0100);
    REQUIRE(buf.size() == 2);
    CHECK(buf.data()[0] == 0x01);
    CHECK(buf.data()[1] == 0x00);

    buf.resetReadCursor();
    CHECK(buf.readUint16BE() == 0x0100);
}

TEST_CASE("PacketBuffer: uint32 big-endian round trip (crcSeed convention)") {
    PacketBuffer buf;
    buf.writeUint32BE(0xDEADBEEF);
    buf.resetReadCursor();
    CHECK(buf.readUint32BE() == 0xDEADBEEF);
}

TEST_CASE("PacketBuffer: ASCII string is u16 length + raw bytes") {
    PacketBuffer buf;
    buf.writeAscii("admin");
    REQUIRE(buf.size() == 2 + 5);
    CHECK(buf.peekUint16(0) == 5);
    CHECK(buf.readAscii() == "admin");
}

TEST_CASE("PacketBuffer: Unicode string is u32 character count + UTF-16LE bytes") {
    std::u16string name = u"Ando";
    PacketBuffer buf;
    buf.writeUnicode(name);
    REQUIRE(buf.size() == 4 + name.size() * 2);
    CHECK(buf.readUnicode() == name);
}

TEST_CASE("PacketBuffer: write-at overwrites without moving the read cursor's data") {
    PacketBuffer buf;
    buf.writeUint16(0x0900); // SOE Data Channel opcode
    buf.writeUint16(0x0000); // sequence placeholder
    buf.writeByte(0xFF);

    buf.writeUint16BEAt(2, 42); // patch in a big-endian sequence number

    CHECK(buf.peekUint16(0) == 0x0900);
    CHECK(buf.peekUint16BE(2) == 42);
    CHECK(buf.peekByte(4) == 0xFF);
}

TEST_CASE("PacketBuffer: removeLastBytes truncates the buffer") {
    PacketBuffer buf;
    buf.writeUint32(0x12345678);
    buf.writeByte(0x01);
    buf.writeUint16BE(0xBEEF);

    buf.removeLastBytes(3);

    CHECK(buf.size() == 4);
    CHECK(buf.readUint32() == 0x12345678);
}

TEST_CASE("PacketBuffer: reading past the end throws") {
    PacketBuffer buf;
    buf.writeByte(1);
    buf.readByte();
    CHECK_THROWS_AS(buf.readByte(), std::out_of_range);
}
