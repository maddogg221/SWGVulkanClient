#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace soe {

// Growable byte buffer with sequential read/write cursors, matching the wire
// conventions Core3 uses (see engine3's Packet.h / ObjectOutputStream.h):
//   - Plain multi-byte fields (opcodes, opCounts, message hashes, most ints)
//     are written in the host's native representation. Core3 runs on
//     little-endian hardware (x86/x64) and does a raw memcpy with no byte
//     swap, so on our own little-endian target the equivalent behavior is
//     "write little-endian" - that's what the plain write*/read* methods do.
//   - A small number of fields are explicitly converted to network byte
//     order in the source (crcSeed in the SOE handshake, and the sequence
//     number in every BasePacket/BaseMessage, via htonl/htons). Those use
//     the explicit *BE methods here.
//   - ASCII strings are uint16 length + raw bytes (no terminator).
//   - Unicode strings are uint32 *character* (UTF-16 code unit) count,
//     followed by UTF-16LE bytes.
class PacketBuffer {
public:
    PacketBuffer() = default;
    explicit PacketBuffer(size_t reserveSize);
    PacketBuffer(const uint8_t* data, size_t len);

    // Sequential writes; append at the end of the buffer.
    void writeByte(uint8_t val);
    void writeUint16(uint16_t val);
    void writeUint32(uint32_t val);
    void writeUint64(uint64_t val);
    void writeFloat(float val);
    void writeUint16BE(uint16_t val);
    void writeUint32BE(uint32_t val);
    void writeAscii(const std::string& str);
    void writeUnicode(const std::u16string& str);
    void writeBytes(const uint8_t* data, size_t len);
    void writeBytes(const std::vector<uint8_t>& bytes);

    // In-place overwrite at an absolute offset. Used to patch fields (e.g.
    // the sequence number) after the rest of the packet has been built.
    void writeByteAt(size_t offset, uint8_t val);
    void writeUint16At(size_t offset, uint16_t val);
    void writeUint16BEAt(size_t offset, uint16_t val);
    void writeUint32At(size_t offset, uint32_t val);
    void writeUint32BEAt(size_t offset, uint32_t val);

    // Sequential reads; advance the read cursor. Throw std::out_of_range if
    // insufficient bytes remain.
    uint8_t readByte();
    uint16_t readUint16();
    uint32_t readUint32();
    uint64_t readUint64();
    float readFloat();
    uint16_t readUint16BE();
    uint32_t readUint32BE();
    std::string readAscii();
    std::u16string readUnicode();
    std::vector<uint8_t> readBytes(size_t len);

    // Absolute reads; do not move the read cursor.
    uint8_t peekByte(size_t offset) const;
    uint16_t peekUint16(size_t offset) const;
    uint16_t peekUint16BE(size_t offset) const;

    const uint8_t* data() const noexcept { return buffer_.data(); }
    uint8_t* data() noexcept { return buffer_.data(); }
    size_t size() const noexcept { return buffer_.size(); }

    // Truncates the buffer, discarding the last `count` bytes (e.g. to
    // strip the trailing compression-flag + CRC bytes after verifying them).
    void removeLastBytes(size_t count);

    // Replaces the buffer's read cursor back to the start without clearing
    // data (mirrors Core3's Packet::reset(), used after rewriting a packet
    // in place during compression).
    void resetReadCursor() noexcept { readPos_ = 0; }

    size_t readPosition() const noexcept { return readPos_; }
    size_t remaining() const noexcept { return buffer_.size() - readPos_; }

private:
    std::vector<uint8_t> buffer_;
    size_t readPos_ = 0;

    void requireReadable(size_t n) const;
    void requireOffset(size_t offset, size_t n) const;
};

} // namespace soe
