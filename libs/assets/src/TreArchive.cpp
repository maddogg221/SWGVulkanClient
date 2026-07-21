#include "assets/TreArchive.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

#include <zlib.h>

namespace assets {

namespace {

// TAG(T,R,E,E) / TAG(0,0,0,5), computed exactly as the original client's
// TAG() macro does (sharedFoundation/Tag.h): each character packed into one
// byte of a big-endian-spelled uint32 constant. On this little-endian
// build, the raw file bytes at these offsets read as "EERT"/"5000" - the
// reversed spelling is expected, not a bug (confirmed via hex dump against
// a real archive).
constexpr uint32_t kTagTree = 0x54524545; // 'TREE'
constexpr uint32_t kTagVersion0005 = 0x30303035; // '0005'

constexpr size_t kMaxReasonableBlockSize = 128u * 1024u * 1024u; // 128MB - generous, still bounded

uint32_t readU32(const uint8_t* p) {
    uint32_t val;
    std::memcpy(&val, p, sizeof(val));
    return val; // native (little-endian) read - matches the header's own raw struct-copy convention
}

std::vector<uint8_t> zlibInflate(const uint8_t* input, size_t inputLen, size_t expectedSize) {
    if (expectedSize == 0 || expectedSize > kMaxReasonableBlockSize) {
        throw std::runtime_error("TreArchive: decompressed size out of reasonable bounds");
    }

    z_stream stream{};
    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error("TreArchive: inflateInit failed");
    }

    std::vector<uint8_t> output(expectedSize);
    stream.next_in = const_cast<Bytef*>(input);
    stream.avail_in = static_cast<uInt>(inputLen);
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    int result = inflate(&stream, Z_FINISH);
    uLong totalOut = stream.total_out;
    inflateEnd(&stream);

    if (result != Z_STREAM_END) {
        throw std::runtime_error("TreArchive: inflate did not finish the stream");
    }
    output.resize(totalOut);
    return output;
}

} // namespace

TreArchive::TreArchive(const std::string& path) : path_(path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("TreArchive: failed to open " + path);
    }

    uint8_t headerBytes[36];
    file.read(reinterpret_cast<char*>(headerBytes), sizeof(headerBytes));
    if (!file || file.gcount() != static_cast<std::streamsize>(sizeof(headerBytes))) {
        throw std::runtime_error("TreArchive: file too short for a header: " + path);
    }

    uint32_t token = readU32(headerBytes + 0);
    uint32_t version = readU32(headerBytes + 4);
    if (token != kTagTree) {
        throw std::runtime_error("TreArchive: bad magic token in " + path);
    }
    if (version != kTagVersion0005) {
        throw std::runtime_error("TreArchive: unsupported version (only 0005 is supported): " + path);
    }

    uint32_t numberOfFiles = readU32(headerBytes + 8);
    uint32_t tocOffset = readU32(headerBytes + 12);
    uint32_t tocCompressor = readU32(headerBytes + 16);
    uint32_t sizeOfTOC = readU32(headerBytes + 20);
    uint32_t blockCompressor = readU32(headerBytes + 24);
    uint32_t sizeOfNameBlock = readU32(headerBytes + 28);
    uint32_t uncompSizeOfNameBlock = readU32(headerBytes + 32);

    constexpr size_t kTocEntrySize = 24; // 6 x int32/uint32 fields, no padding
    if (sizeOfTOC > kMaxReasonableBlockSize || sizeOfNameBlock > kMaxReasonableBlockSize) {
        throw std::runtime_error("TreArchive: TOC/name block size out of reasonable bounds: " + path);
    }

    // Read + decompress the TOC block.
    std::vector<uint8_t> tocRaw(sizeOfTOC);
    file.seekg(tocOffset, std::ios::beg);
    file.read(reinterpret_cast<char*>(tocRaw.data()), sizeOfTOC);
    if (!file || file.gcount() != static_cast<std::streamsize>(sizeOfTOC)) {
        throw std::runtime_error("TreArchive: failed to read TOC block: " + path);
    }
    std::vector<uint8_t> tocDecompressed;
    if (tocCompressor != 0) {
        tocDecompressed = zlibInflate(tocRaw.data(), tocRaw.size(),
                                       static_cast<size_t>(numberOfFiles) * kTocEntrySize);
    } else {
        tocDecompressed = std::move(tocRaw);
    }
    if (tocDecompressed.size() != static_cast<size_t>(numberOfFiles) * kTocEntrySize) {
        throw std::runtime_error("TreArchive: TOC size mismatch after decompression: " + path);
    }

    // Read + decompress the name block (immediately follows the TOC block).
    std::vector<uint8_t> nameRaw(sizeOfNameBlock);
    file.read(reinterpret_cast<char*>(nameRaw.data()), sizeOfNameBlock);
    if (!file || file.gcount() != static_cast<std::streamsize>(sizeOfNameBlock)) {
        throw std::runtime_error("TreArchive: failed to read name block: " + path);
    }
    std::vector<uint8_t> nameDecompressed;
    if (blockCompressor != 0) {
        nameDecompressed = zlibInflate(nameRaw.data(), nameRaw.size(), uncompSizeOfNameBlock);
    } else {
        nameDecompressed = std::move(nameRaw);
    }
    if (nameDecompressed.size() != uncompSizeOfNameBlock) {
        throw std::runtime_error("TreArchive: name block size mismatch after decompression: " + path);
    }

    // Parse TOC entries and pair each with its NUL-terminated name.
    entriesByName_.reserve(numberOfFiles);
    for (uint32_t i = 0; i < numberOfFiles; ++i) {
        const uint8_t* entryBytes = tocDecompressed.data() + static_cast<size_t>(i) * kTocEntrySize;
        TocEntry entry;
        entry.crc = readU32(entryBytes + 0);
        entry.length = static_cast<int32_t>(readU32(entryBytes + 4));
        entry.offset = static_cast<int32_t>(readU32(entryBytes + 8));
        entry.compressor = static_cast<int32_t>(readU32(entryBytes + 12));
        entry.compressedLength = static_cast<int32_t>(readU32(entryBytes + 16));
        entry.fileNameOffset = static_cast<int32_t>(readU32(entryBytes + 20));

        if (entry.length == 0) {
            continue; // a deleted entry (matches TreeFile_SearchNode.cpp's own convention)
        }
        if (entry.fileNameOffset < 0 ||
            static_cast<size_t>(entry.fileNameOffset) >= nameDecompressed.size()) {
            throw std::runtime_error("TreArchive: fileNameOffset out of range: " + path);
        }

        const char* namePtr =
            reinterpret_cast<const char*>(nameDecompressed.data() + entry.fileNameOffset);
        size_t maxLen = nameDecompressed.size() - static_cast<size_t>(entry.fileNameOffset);
        size_t nameLen = strnlen(namePtr, maxLen);
        entriesByName_.emplace(std::string(namePtr, nameLen), entry);
    }
}

bool TreArchive::contains(const std::string& fileName) const {
    return entriesByName_.find(fileName) != entriesByName_.end();
}

std::vector<uint8_t> TreArchive::extract(const std::string& fileName) const {
    auto it = entriesByName_.find(fileName);
    if (it == entriesByName_.end()) {
        throw std::runtime_error("TreArchive: file not found: " + fileName);
    }
    const TocEntry& entry = it->second;

    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        throw std::runtime_error("TreArchive: failed to reopen " + path_);
    }

    if (entry.compressor == 0) {
        std::vector<uint8_t> raw(static_cast<size_t>(entry.length));
        file.seekg(entry.offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(raw.data()), entry.length);
        if (!file || file.gcount() != static_cast<std::streamsize>(entry.length)) {
            throw std::runtime_error("TreArchive: failed to read file data: " + fileName);
        }
        return raw;
    }

    std::vector<uint8_t> compressed(static_cast<size_t>(entry.compressedLength));
    file.seekg(entry.offset, std::ios::beg);
    file.read(reinterpret_cast<char*>(compressed.data()), entry.compressedLength);
    if (!file || file.gcount() != static_cast<std::streamsize>(entry.compressedLength)) {
        throw std::runtime_error("TreArchive: failed to read compressed file data: " + fileName);
    }
    return zlibInflate(compressed.data(), compressed.size(), static_cast<size_t>(entry.length));
}

std::vector<std::string> TreArchive::listFiles() const {
    std::vector<std::string> names;
    names.reserve(entriesByName_.size());
    for (const auto& [name, entry] : entriesByName_) {
        names.push_back(name);
    }
    return names;
}

} // namespace assets
