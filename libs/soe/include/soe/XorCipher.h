#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"

namespace soe {

// 4-byte-block chained XOR cipher keyed by the connection's crcSeed, used to
// obfuscate SOE packet payloads. Ported from engine3's
// BaseProtocol::encrypt/decrypt (BaseProtocol.cpp).
//
// Both directions skip the packet's leading 1 or 2 bytes (the SOE opcode
// marker: 2 bytes if the very first byte is 0x00, else 1 byte) AND exclude
// the trailing 2 bytes (reserved for the packet CRC, which is appended
// after encryption on send and verified before decryption on receive, so
// those 2 bytes are never actually encrypted on the wire). The remainder is
// XORed in 4-byte little-endian blocks, chaining the seed forward from the
// ciphertext of each block. Any trailing 0-3 bytes that don't fill a full
// block are each XORed with the low byte of the seed left over from the
// last full block (Core3 does not re-chain per remainder byte - that quirk
// is preserved here since both sides must agree bit-for-bit).
class XorCipher {
public:
    static void encrypt(PacketBuffer& buffer, uint32_t crcSeed);
    static void decrypt(PacketBuffer& buffer, uint32_t crcSeed);
};

} // namespace soe
