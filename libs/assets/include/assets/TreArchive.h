#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace assets {

// Reads a Star Wars Galaxies client ".tre" asset archive - real client
// files, e.g. C:\Program Files (x86)\StarWarsGalaxies\data_static_mesh_00.tre.
//
// Format confirmed directly against the ORIGINAL client engine source
// (TreeFile::SearchTree in a local sparse clone of
// github.com/SWG-Source/src, engine/shared/library/sharedFile/src/shared/
// TreeFile_SearchNode.{h,cpp}) and cross-checked byte-for-byte against a
// real archive on disk - not guessed from prose docs. Layout:
//   - 36-byte header (all fields plain native/little-endian uint32 - NOT
//     the big-endian convention IffReader's chunk format uses; this is a
//     separate, SOE-proprietary archive format with no such legacy
//     convention to honor): `token` ('TREE', stored as raw bytes "EERT" on
//     a little-endian host - confirmed via hex dump), `version` ('0005' -
//     the only version this project supports, matching every real archive
//     found on this machine), `numberOfFiles`, `tocOffset`, `tocCompressor`,
//     `sizeOfTOC`, `blockCompressor`, `sizeOfNameBlock`,
//     `uncompSizeOfNameBlock`.
//   - At `tocOffset`: `sizeOfTOC` bytes, zlib-compressed if `tocCompressor
//     != 0`, decompressing to `numberOfFiles` 24-byte TableOfContentsEntry
//     records (crc, length, offset, compressor, compressedLength,
//     fileNameOffset - all native int32/uint32, no padding).
//   - Immediately after: `sizeOfNameBlock` bytes, zlib-compressed if
//     `blockCompressor != 0`, decompressing to `uncompSizeOfNameBlock`
//     bytes of NUL-terminated filenames; each TOC entry's `fileNameOffset`
//     indexes directly into this buffer.
//   - Each entry's own file data lives at `offset` in the archive: `length`
//     raw bytes if `compressor == 0`, else `compressedLength` zlib bytes
//     decompressing to `length` bytes.
// The original engine binary-searches the CRC-sorted TOC (crc = the same
// CRC-32 table/algorithm this project already ported as
// soe::MessageHash::compute() - confirmed identical table constants read
// directly from Crc.cpp); this reader instead builds a plain
// name -> entry map once at open time, since random-access extraction (not
// runtime binary search) is all this project needs.
class TreArchive {
public:
    // Throws std::runtime_error if the file can't be opened, is too short,
    // has the wrong magic token, or is a version other than 0005.
    explicit TreArchive(const std::string& path);

    bool contains(const std::string& fileName) const;

    // Extracts and returns the named file's raw, decompressed bytes.
    // Throws std::runtime_error if `fileName` isn't in this archive.
    std::vector<uint8_t> extract(const std::string& fileName) const;

    // Every filename this archive contains, in TOC order - e.g. for
    // bulk-searching candidate template paths against a live archive.
    std::vector<std::string> listFiles() const;

    size_t fileCount() const { return entriesByName_.size(); }

private:
    struct TocEntry {
        uint32_t crc = 0;
        int32_t length = 0;
        int32_t offset = 0;
        int32_t compressor = 0;
        int32_t compressedLength = 0;
        int32_t fileNameOffset = 0;
    };

    std::string path_;
    std::unordered_map<std::string, TocEntry> entriesByName_;
};

} // namespace assets
