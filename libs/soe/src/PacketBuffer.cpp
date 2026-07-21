#include "soe/PacketBuffer.h"

#include <cstring>
#include <stdexcept>

namespace soe {

PacketBuffer::PacketBuffer(size_t reserveSize) {
    buffer_.reserve(reserveSize);
}

PacketBuffer::PacketBuffer(const uint8_t* data, size_t len)
    : buffer_(data, data + len) {
}

void PacketBuffer::requireReadable(size_t n) const {
    if (remaining() < n) {
        throw std::out_of_range("PacketBuffer: read past end of buffer");
    }
}

void PacketBuffer::requireOffset(size_t offset, size_t n) const {
    // Written to avoid overflowing offset+n (which would defeat the check for
    // a huge offset produced by an underflowed size_t subtraction upstream).
    if (n > buffer_.size() || offset > buffer_.size() - n) {
        throw std::out_of_range("PacketBuffer: write-at offset out of range");
    }
}

// ---- sequential writes ----

void PacketBuffer::writeByte(uint8_t val) {
    buffer_.push_back(val);
}

void PacketBuffer::writeUint16(uint16_t val) {
    buffer_.push_back(static_cast<uint8_t>(val & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void PacketBuffer::writeUint32(uint32_t val) {
    buffer_.push_back(static_cast<uint8_t>(val & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void PacketBuffer::writeUint64(uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        buffer_.push_back(static_cast<uint8_t>((val >> (8 * i)) & 0xFF));
    }
}

void PacketBuffer::writeFloat(float val) {
    uint32_t bits;
    static_assert(sizeof(bits) == sizeof(val), "float must be 32 bits");
    std::memcpy(&bits, &val, sizeof(bits));
    writeUint32(bits);
}

void PacketBuffer::writeUint16BE(uint16_t val) {
    buffer_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(val & 0xFF));
}

void PacketBuffer::writeUint32BE(uint32_t val) {
    buffer_.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(val & 0xFF));
}

void PacketBuffer::writeAscii(const std::string& str) {
    writeUint16(static_cast<uint16_t>(str.size()));
    writeBytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void PacketBuffer::writeUnicode(const std::u16string& str) {
    writeUint32(static_cast<uint32_t>(str.size()));
    for (char16_t ch : str) {
        writeUint16(static_cast<uint16_t>(ch));
    }
}

void PacketBuffer::writeBytes(const uint8_t* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

void PacketBuffer::writeBytes(const std::vector<uint8_t>& bytes) {
    writeBytes(bytes.data(), bytes.size());
}

// ---- in-place overwrite ----

void PacketBuffer::writeByteAt(size_t offset, uint8_t val) {
    requireOffset(offset, 1);
    buffer_[offset] = val;
}

void PacketBuffer::writeUint16At(size_t offset, uint16_t val) {
    requireOffset(offset, 2);
    buffer_[offset] = static_cast<uint8_t>(val & 0xFF);
    buffer_[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

void PacketBuffer::writeUint16BEAt(size_t offset, uint16_t val) {
    requireOffset(offset, 2);
    buffer_[offset] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buffer_[offset + 1] = static_cast<uint8_t>(val & 0xFF);
}

void PacketBuffer::writeUint32At(size_t offset, uint32_t val) {
    requireOffset(offset, 4);
    buffer_[offset] = static_cast<uint8_t>(val & 0xFF);
    buffer_[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buffer_[offset + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buffer_[offset + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

void PacketBuffer::writeUint32BEAt(size_t offset, uint32_t val) {
    requireOffset(offset, 4);
    buffer_[offset] = static_cast<uint8_t>((val >> 24) & 0xFF);
    buffer_[offset + 1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buffer_[offset + 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buffer_[offset + 3] = static_cast<uint8_t>(val & 0xFF);
}

// ---- sequential reads ----

uint8_t PacketBuffer::readByte() {
    requireReadable(1);
    return buffer_[readPos_++];
}

uint16_t PacketBuffer::readUint16() {
    requireReadable(2);
    uint16_t val = static_cast<uint16_t>(buffer_[readPos_]) |
                   (static_cast<uint16_t>(buffer_[readPos_ + 1]) << 8);
    readPos_ += 2;
    return val;
}

uint32_t PacketBuffer::readUint32() {
    requireReadable(4);
    uint32_t val = static_cast<uint32_t>(buffer_[readPos_]) |
                   (static_cast<uint32_t>(buffer_[readPos_ + 1]) << 8) |
                   (static_cast<uint32_t>(buffer_[readPos_ + 2]) << 16) |
                   (static_cast<uint32_t>(buffer_[readPos_ + 3]) << 24);
    readPos_ += 4;
    return val;
}

uint64_t PacketBuffer::readUint64() {
    requireReadable(8);
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= static_cast<uint64_t>(buffer_[readPos_ + i]) << (8 * i);
    }
    readPos_ += 8;
    return val;
}

float PacketBuffer::readFloat() {
    uint32_t bits = readUint32();
    float val;
    static_assert(sizeof(bits) == sizeof(val), "float must be 32 bits");
    std::memcpy(&val, &bits, sizeof(val));
    return val;
}

uint16_t PacketBuffer::readUint16BE() {
    requireReadable(2);
    uint16_t val = (static_cast<uint16_t>(buffer_[readPos_]) << 8) |
                   static_cast<uint16_t>(buffer_[readPos_ + 1]);
    readPos_ += 2;
    return val;
}

uint32_t PacketBuffer::readUint32BE() {
    requireReadable(4);
    uint32_t val = (static_cast<uint32_t>(buffer_[readPos_]) << 24) |
                   (static_cast<uint32_t>(buffer_[readPos_ + 1]) << 16) |
                   (static_cast<uint32_t>(buffer_[readPos_ + 2]) << 8) |
                   static_cast<uint32_t>(buffer_[readPos_ + 3]);
    readPos_ += 4;
    return val;
}

std::string PacketBuffer::readAscii() {
    uint16_t len = readUint16();
    requireReadable(len);
    std::string str(reinterpret_cast<const char*>(&buffer_[readPos_]), len);
    readPos_ += len;
    return str;
}

std::u16string PacketBuffer::readUnicode() {
    uint32_t count = readUint32();
    std::u16string str;
    str.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        str.push_back(static_cast<char16_t>(readUint16()));
    }
    return str;
}

std::vector<uint8_t> PacketBuffer::readBytes(size_t len) {
    requireReadable(len);
    std::vector<uint8_t> out(buffer_.begin() + readPos_, buffer_.begin() + readPos_ + len);
    readPos_ += len;
    return out;
}

// ---- absolute reads ----

uint8_t PacketBuffer::peekByte(size_t offset) const {
    requireOffset(offset, 1);
    return buffer_[offset];
}

uint16_t PacketBuffer::peekUint16(size_t offset) const {
    requireOffset(offset, 2);
    return static_cast<uint16_t>(buffer_[offset]) |
           (static_cast<uint16_t>(buffer_[offset + 1]) << 8);
}

uint16_t PacketBuffer::peekUint16BE(size_t offset) const {
    requireOffset(offset, 2);
    return (static_cast<uint16_t>(buffer_[offset]) << 8) |
           static_cast<uint16_t>(buffer_[offset + 1]);
}

// ---- buffer management ----

void PacketBuffer::removeLastBytes(size_t count) {
    if (count > buffer_.size()) {
        throw std::out_of_range("PacketBuffer: removeLastBytes count exceeds buffer size");
    }
    buffer_.resize(buffer_.size() - count);
    if (readPos_ > buffer_.size()) {
        readPos_ = buffer_.size();
    }
}

} // namespace soe
