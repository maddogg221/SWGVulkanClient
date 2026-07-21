#include "assets/StaticMesh.h"

#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {

constexpr uint32_t kSpsTag = 0x53505320;  // 'SPS ' (Shader Primitive Set)
constexpr uint32_t kVtxaTag = 0x56545841; // 'VTXA' (vertex array)
constexpr uint32_t kInfoTag = 0x494E464F; // 'INFO'
constexpr uint32_t kDataTag = 0x44415441; // 'DATA'
constexpr uint32_t kIndxTag = 0x494E4458; // 'INDX'
constexpr uint32_t kNameTag = 0x4E414D45; // 'NAME'

// Per-vertex layout for the 11 real vertex sizes this format uses (ported
// from meshLib's mshVertex::getTexCoords()/getPosition()/getNormal() -
// github.com/apathyboy/meshlib, a real, working reference implementation
// for this exact format - confirmed against real data: this project's own
// first real mesh hit the unambiguous 32-byte case, position/normal/UV0
// exactly where this table says). Position (3 floats) and normal (3
// floats) are always the first 24 bytes; a packed ARGB color (4 bytes, not
// read by this project - out of scope) optionally follows for the
// "+4" sizes; UV0 (2 floats) is always the first texture-coordinate pair,
// at the byte offset below. Sizes with more than one UV set (40/44/56/60/
// 64/68/72) still only have UV0 read here, matching this pass' "one UV set"
// scope - the rest of those vertices' bytes are simply never touched.
bool uv0ByteOffsetFor(size_t bytesPerVertex, size_t& outOffset) {
    switch (bytesPerVertex) {
        case 32: outOffset = 24; return true;
        case 36: outOffset = 28; return true;
        case 40: outOffset = 24; return true;
        case 44: outOffset = 28; return true;
        case 48: outOffset = 24; return true;
        case 52: outOffset = 28; return true;
        case 56: outOffset = 24; return true;
        case 60: outOffset = 28; return true;
        case 64: outOffset = 24; return true;
        case 68: outOffset = 28; return true;
        case 72: outOffset = 24; return true;
        default: return false;
    }
}

} // namespace

namespace {

// The VTXA INFO chunk (fvfCodes + numVerts) is exactly 8 bytes - the ONLY
// 8-byte INFO chunk in a real mesh (a geometry-level INFO chunk seen
// alongside it is 6 bytes, a different, unrelated field this project
// doesn't read). Searched by tag+size together since IffChunk doesn't
// disambiguate same-tag chunks by content otherwise.
const IffChunk* findVtxaInfo(const IffChunk& chunk) {
    if (chunk.id == kInfoTag && chunk.data.size() == 8) {
        return &chunk;
    }
    for (const auto& child : chunk.children) {
        if (const IffChunk* found = findVtxaInfo(child)) {
            return found;
        }
    }
    return nullptr;
}

// Parses one real shader-group submesh (a FORM directly under SPS's version
// wrapper - see StaticMesh::parse's own comment for the real chunk shape).
MeshData parseSubmeshGeometry(const IffChunk& submeshForm) {
    const IffChunk* vtxa = findFirstForm(submeshForm, kVtxaTag);
    if (vtxa == nullptr) {
        throw std::runtime_error("StaticMesh::parse: no VTXA (vertex array) found");
    }
    const IffChunk* indx = findFirstChunk(submeshForm, kIndxTag);
    if (indx == nullptr) {
        throw std::runtime_error("StaticMesh::parse: no INDX (index array) found");
    }

    const IffChunk* vtxaInfo = findVtxaInfo(*vtxa);
    if (vtxaInfo == nullptr) {
        throw std::runtime_error("StaticMesh::parse: VTXA INFO (fvf codes/vertex count) not found");
    }

    soe::PacketBuffer infoBuf = vtxaInfo->data;
    infoBuf.resetReadCursor();
    infoBuf.readUint32(); // fvfCodes - not used; vertex layout is derived from size instead
                           // (see uv0ByteOffsetFor()'s comment for why)
    uint32_t numVerts = infoBuf.readUint32();

    const IffChunk* vertexData = findFirstChunk(*vtxa, kDataTag);
    if (vertexData == nullptr) {
        throw std::runtime_error("StaticMesh::parse: vertex DATA chunk not found");
    }
    if (numVerts == 0 || vertexData->data.size() % numVerts != 0) {
        throw std::runtime_error("StaticMesh::parse: vertex DATA size isn't a multiple of numVerts");
    }
    size_t bytesPerVertex = vertexData->data.size() / numVerts;
    size_t uv0Offset = 0;
    if (!uv0ByteOffsetFor(bytesPerVertex, uv0Offset)) {
        throw std::runtime_error("StaticMesh::parse: unsupported vertex size " +
                                  std::to_string(bytesPerVertex));
    }

    MeshData result;
    result.positions.reserve(numVerts);
    result.normals.reserve(numVerts);
    result.uv0.reserve(numVerts);

    soe::PacketBuffer vBuf = vertexData->data;
    vBuf.resetReadCursor();
    for (uint32_t i = 0; i < numVerts; ++i) {
        Float3 position;
        position.x = vBuf.readFloat();
        position.y = vBuf.readFloat();
        position.z = vBuf.readFloat();
        Float3 normal;
        normal.x = vBuf.readFloat();
        normal.y = vBuf.readFloat();
        normal.z = vBuf.readFloat();
        result.positions.push_back(position);
        result.normals.push_back(normal);

        // Skip to this vertex's UV0 offset (may skip a packed color field),
        // read UV0, then skip the rest of this vertex's remaining bytes -
        // vBuf's cursor is currently at byte 24 within this vertex (just
        // read pos+normal), so the remaining skip is relative to that.
        size_t alreadyRead = 24;
        vBuf.readBytes(uv0Offset - alreadyRead);
        Float2 uv;
        uv.x = vBuf.readFloat();
        uv.y = vBuf.readFloat();
        result.uv0.push_back(uv);

        size_t consumedThisVertex = uv0Offset + 8;
        if (consumedThisVertex < bytesPerVertex) {
            vBuf.readBytes(bytesPerVertex - consumedThisVertex);
        }
    }

    // INDX: 4-byte count, then that many indices - 2 bytes each for every
    // real mesh checked this session, but computed generically (matches
    // meshLib's own readGeometryINDX: bytesPerIndex = (size-4)/count)
    // rather than assumed fixed.
    soe::PacketBuffer iBuf = indx->data;
    iBuf.resetReadCursor();
    uint32_t numIndex = iBuf.readUint32();
    if (numIndex == 0 || (indx->data.size() - 4) % numIndex != 0) {
        throw std::runtime_error("StaticMesh::parse: INDX size isn't a multiple of its own count");
    }
    size_t bytesPerIndex = (indx->data.size() - 4) / numIndex;
    result.indices.reserve(numIndex);
    for (uint32_t i = 0; i < numIndex; ++i) {
        if (bytesPerIndex == 2) {
            result.indices.push_back(iBuf.readUint16());
        } else if (bytesPerIndex == 4) {
            result.indices.push_back(iBuf.readUint32());
        } else {
            throw std::runtime_error("StaticMesh::parse: unsupported index byte width");
        }
    }

    return result;
}

} // namespace

std::vector<StaticMeshSubmesh> StaticMesh::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != 0x4D455348) {
        throw std::runtime_error("StaticMesh::parse: not a FORM(MESH)-rooted file");
    }

    const IffChunk* sps = findFirstForm(topLevel[0], kSpsTag);
    if (sps == nullptr) {
        throw std::runtime_error("StaticMesh::parse: no SPS (Shader Primitive Set) found");
    }

    // SPS wraps a single version FORM (e.g. "0001") whose own children are
    // one FORM per real shader group actually used by this mesh - confirmed
    // live via a direct chunk-tree dump: a real elevator platform mesh has
    // 5 (floor marble, two medical lights, a roof trim, a wall fallback),
    // each with its own NAME (shader path) + real geometry. Only SPS itself
    // is guaranteed singular; every FORM child of its version wrapper is a
    // real submesh to read (a leading CNT leaf chunk giving the submesh
    // count sits alongside them but isn't needed - the FORM children are
    // simply iterated directly, same style BuildingLayout.cpp already uses
    // for its own repeated CELS children).
    if (sps->children.empty() || sps->children[0].id != kFormTag) {
        throw std::runtime_error("StaticMesh::parse: SPS has no version wrapper FORM");
    }
    const IffChunk& spsVersion = sps->children[0];

    std::vector<StaticMeshSubmesh> result;
    for (const IffChunk& submeshForm : spsVersion.children) {
        if (submeshForm.id != kFormTag) {
            continue; // the CNT count leaf, not a submesh
        }
        const IffChunk* nameChunk = findFirstChunk(submeshForm, kNameTag);
        if (nameChunk == nullptr) {
            throw std::runtime_error("StaticMesh::parse: submesh missing NAME (shader) chunk");
        }
        soe::PacketBuffer nameBuf = nameChunk->data;
        nameBuf.resetReadCursor();

        StaticMeshSubmesh submesh;
        submesh.shaderFilename = readNulTerminatedString(nameBuf);
        submesh.mesh = parseSubmeshGeometry(submeshForm);
        result.push_back(std::move(submesh));
    }

    return result;
}

} // namespace assets
