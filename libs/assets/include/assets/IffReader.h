#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"

namespace assets {

// The 'FORM' tag (0x464F524D) - a chunk with this id carries a 4-byte
// "form type" subtype tag followed by nested child chunks, instead of raw
// leaf data. Matches engine3's Chunk::FORM constant exactly.
constexpr uint32_t kFormTag = 0x464F524D;

// The 'XXXX' tag (0x58585858) - a self-describing "named parameter" leaf
// chunk used throughout SharedObjectTemplate/AppearanceTemplate-style asset
// files (.iff/.apt): every field is stored as one XXXX chunk whose data is a
// NUL-terminated ASCII parameter NAME (e.g. "appearanceFilename") followed
// by the parameter's own value (a NUL-terminated string for filename-typed
// fields, other binary encodings for other types - only string-valued
// fields are ever read by this project so far). Confirmed by directly
// dumping a real extracted .iff's chunk tree and finding readable
// "appearanceFilename\0appearance/thm_prp_crate_spice.apt\0" bytes inside
// an XXXX chunk - not documented anywhere, discovered empirically against
// real data.
constexpr uint32_t kParamTag = 0x58585858;

// Reads a NUL-terminated ASCII string starting at the buffer's current read
// position, advancing the cursor past the terminator. Mirrors
// engine3's Chunk::readString(). Throws std::out_of_range (via
// soe::PacketBuffer::readByte()) if no NUL byte is found before the buffer
// ends.
std::string readNulTerminatedString(soe::PacketBuffer& buf);

// One node of a parsed IFF chunk tree. Every SWG client asset this project
// cares about (.iff templates, .apt appearance templates, .msh meshes) uses
// this same container format - a classic Amiga-IFF-style tagged chunk tree.
//
// If `id == kFormTag`, this is a FORM: `formType` is its own 4-char subtype
// tag and `children` holds its nested chunks (already recursively parsed);
// `data` is left empty. Otherwise this is a leaf/data chunk: `data` holds
// its raw payload, ready to read sequentially via soe::PacketBuffer's own
// read*() methods; `formType`/`children` are unused.
struct IffChunk {
    uint32_t id = 0;
    uint32_t formType = 0;
    std::vector<IffChunk> children;
    soe::PacketBuffer data;

    // The first direct child whose id matches, or nullptr if none. Only
    // searches direct children, matching how these formats are consumed in
    // practice (each level's caller already knows which FORM it's inside).
    const IffChunk* findChild(uint32_t childId) const;
};

// Recursive search for the first descendant FORM whose formType matches -
// distinct from findChild() above, which matches by id (every FORM's own id
// is always kFormTag, never useful to search by directly). Originally a
// StaticMesh.cpp-local helper (needed when a FORM search by id silently
// never matched); promoted here once a second, unrelated parser
// (terrain::TrnFile) needed the identical logic.
const IffChunk* findFirstForm(const IffChunk& chunk, uint32_t formTypeTag);

// Depth-first (pre-order, including `chunk` itself) search for the first
// descendant LEAF chunk (id != kFormTag) whose id matches - distinct from
// findFirstForm() above, which matches a FORM's formType, never a leaf's own
// id. Originally a StaticMesh.cpp-local helper; promoted here once the
// skeletal-mesh parsers (Skeleton.cpp/SkeletalMesh.cpp) needed the identical
// logic to look past version-numbered wrapper FORMs (e.g. '0002'/'0004')
// whose exact tag isn't assumed stable.
const IffChunk* findFirstChunk(const IffChunk& chunk, uint32_t tag);

// Collects every descendant FORM whose formType matches (including `chunk`
// itself), WITHOUT descending further into a match once found - real
// skeletal-mesh formats nest repeated same-type FORMs as siblings (e.g. a
// `.skt`'s multiple LOD-level `FORM SKTM` blocks, a `.mgn`'s multiple
// per-shader `FORM PSDT` blocks), never one inside another, so this is safe
// as a "collect the repeated group" helper distinct from findFirstForm()'s
// single-match search.
std::vector<const IffChunk*> findAllForms(const IffChunk& chunk, uint32_t formTypeTag);

// Parses a raw asset file buffer into a tree of chunks. Ported from
// engine3's IffStream/Chunk/Form (see
// utils/engine3/MMOEngine/src/engine/util/iffstream/ in the Core3 reference
// clone) - confirmed there that chunk tag and size fields are both a plain
// 4-byte big-endian read (IffStream::loadMainChunks/Form::parseSubObjects
// both use readNetInt()/htonl()), and that a chunk's data occupies exactly
// its declared `size` bytes with no padding between chunks. Unlike
// engine3's version (which uses a factory to construct typed Chunk
// subclasses per registered chunk ID, since Core3 needs full read/write
// object semantics), this only ever needs to READ data, so every chunk is
// the same plain IffChunk struct - no factory/registration machinery.
class IffReader {
public:
    // Throws std::out_of_range (via soe::PacketBuffer) if the buffer is
    // truncated or a declared chunk size overruns what's left - the same
    // "malformed input throws" convention swgproto's own decoders already
    // use, not a silent best-effort parse.
    static std::vector<IffChunk> parse(const std::vector<uint8_t>& bytes);

private:
    static IffChunk parseOneChunk(soe::PacketBuffer& buf);
};

} // namespace assets
