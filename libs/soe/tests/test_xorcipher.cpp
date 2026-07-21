#include <vector>

#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"
#include "soe/XorCipher.h"

using soe::PacketBuffer;
using soe::XorCipher;

namespace {

PacketBuffer buildDataChannelPacket() {
    PacketBuffer buf;
    buf.writeUint16(0x0900);   // SOE Data Channel opcode (first byte 0x00)
    buf.writeUint16BE(7);      // sequence
    buf.writeUint16(0x04);     // opCount
    buf.writeUint32(0x41131F96); // message hash
    buf.writeAscii("admin");
    buf.writeAscii("hunter2");
    buf.writeAscii("20050408-18:00");
    buf.writeUint32(0xDEADBEEF); // trailing int field, to exercise multi-block chaining
    buf.writeByte(0x00);        // compression flag
    buf.writeUint16(0);         // CRC placeholder
    return buf;
}

} // namespace

TEST_CASE("XorCipher: decrypt(encrypt(x)) restores the original bytes") {
    PacketBuffer buf = buildDataChannelPacket();
    std::vector<uint8_t> original(buf.data(), buf.data() + buf.size());

    XorCipher::encrypt(buf, 0x12345678);

    // Encryption must have actually changed the payload bytes (anything
    // between the 2-byte header and the trailing 2 CRC bytes).
    bool anyByteChanged = false;
    for (size_t i = 2; i < buf.size() - 2; ++i) {
        if (buf.data()[i] != original[i]) {
            anyByteChanged = true;
            break;
        }
    }
    CHECK(anyByteChanged);

    XorCipher::decrypt(buf, 0x12345678);

    std::vector<uint8_t> roundTripped(buf.data(), buf.data() + buf.size());
    CHECK(roundTripped == original);
}

TEST_CASE("XorCipher: header bytes and trailing CRC placeholder are never touched") {
    PacketBuffer buf = buildDataChannelPacket();
    std::vector<uint8_t> original(buf.data(), buf.data() + buf.size());

    XorCipher::encrypt(buf, 0xABCDEF01);

    CHECK(buf.data()[0] == original[0]);
    CHECK(buf.data()[1] == original[1]);
    CHECK(buf.data()[buf.size() - 1] == original[buf.size() - 1]);
    CHECK(buf.data()[buf.size() - 2] == original[buf.size() - 2]);
}

TEST_CASE("XorCipher: a wrong seed fails to reproduce the original") {
    PacketBuffer buf = buildDataChannelPacket();
    std::vector<uint8_t> original(buf.data(), buf.data() + buf.size());

    XorCipher::encrypt(buf, 0x12345678);
    XorCipher::decrypt(buf, 0x00000000); // wrong seed

    std::vector<uint8_t> roundTripped(buf.data(), buf.data() + buf.size());
    CHECK(roundTripped != original);
}

TEST_CASE("XorCipher: 1-byte-marker packets (first byte != 0x00) round-trip too") {
    PacketBuffer buf;
    buf.writeByte(0x06); // e.g. a single-byte SOE opcode form
    buf.writeAscii("payload-data-here");
    buf.writeByte(0x00);
    buf.writeUint16(0);

    std::vector<uint8_t> original(buf.data(), buf.data() + buf.size());

    XorCipher::encrypt(buf, 0x9999AAAA);
    XorCipher::decrypt(buf, 0x9999AAAA);

    std::vector<uint8_t> roundTripped(buf.data(), buf.data() + buf.size());
    CHECK(roundTripped == original);
}
