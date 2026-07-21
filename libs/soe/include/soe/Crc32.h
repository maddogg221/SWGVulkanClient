#pragma once

#include <cstddef>
#include <cstdint>

#include "soe/PacketBuffer.h"

namespace soe {

// The "packet CRC": a seeded CRC-32 used to validate SOE packets end-to-end.
// This is a DIFFERENT algorithm from MessageHash (String::hashCode, used for
// game-message opcodes) - both use CRC-32-family tables but with different
// bit order and seeding, and are not interchangeable.
// Ported from engine3's BaseProtocol::generateCrc/appendCRC/testCRC.
class PacketCrc {
public:
    // Computes the seeded CRC over the first `length` bytes of `data`,
    // seeded by the connection's crcSeed (from the SOE handshake).
    static uint32_t generate(const uint8_t* data, size_t length, uint32_t crcSeed);

    // Appends `crcLength` CRC bytes (big-endian) to the end of `buffer`,
    // computed over the buffer's existing contents. Call this last, after
    // compression and encryption have already been applied.
    static void append(PacketBuffer& buffer, uint32_t crcSeed, uint16_t crcLength = 2);

    // Verifies the trailing `crcLength` bytes of `buffer` match the CRC of
    // everything preceding them. Call this first, before decryption, on a
    // freshly-received packet.
    static bool test(const PacketBuffer& buffer, uint32_t crcSeed, uint16_t crcLength = 2);
};

} // namespace soe
