#include "assets/SkeletalMesh.h"

#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {

constexpr uint32_t kSkmgTag = 0x534B4D47;     // 'SKMG' (as the root FORM's formType)
constexpr uint32_t kInfoTag = 0x494E464F;     // 'INFO'
constexpr uint32_t kSktmTag = 0x534B544D;     // 'SKTM' (a leaf chunk here, not a FORM)
constexpr uint32_t kXfnmTag = 0x58464E4D;     // 'XFNM'
constexpr uint32_t kPosnTag = 0x504F534E;     // 'POSN'
constexpr uint32_t kTwhdTag = 0x54574844;     // 'TWHD'
constexpr uint32_t kTwdtTag = 0x54574454;     // 'TWDT'
constexpr uint32_t kNormTag = 0x4E4F524D;     // 'NORM'
constexpr uint32_t kPsdtFormTag = 0x50534454; // 'PSDT'
constexpr uint32_t kNameTag = 0x4E414D45;     // 'NAME'
constexpr uint32_t kPidxTag = 0x50494458;     // 'PIDX'
constexpr uint32_t kNidxTag = 0x4E494458;     // 'NIDX'
constexpr uint32_t kTcsfFormTag = 0x54435346; // 'TCSF'
constexpr uint32_t kTcsdTag = 0x54435344;     // 'TCSD'
constexpr uint32_t kPrimFormTag = 0x5052494D; // 'PRIM'
constexpr uint32_t kItlTag = 0x49544C20;      // 'ITL ' (trailing space)
constexpr uint32_t kOitlTag = 0x4F49544C;     // 'OITL' - "organized"/grouped triangle
                                               // list, a real alternative to plain ITL
                                               // seen in more complex real meshes (e.g.
                                               // every zab_m_*.mgn player-race body
                                               // part, confirmed live - simpler
                                               // creatures like gubbur/tanc_mite use
                                               // plain ITL instead). Same per-triangle
                                               // vertex-index data, just additionally
                                               // tagged with a per-triangle "group"
                                               // (int16) - real, presumably used for
                                               // occlusion-zone/LOD culling in the
                                               // original renderer, not needed for
                                               // single-index-buffer bind-pose
                                               // rendering, so groups are flattened
                                               // into one combined triangle list here.

// The mesh-level INFO chunk is always 44 bytes - the ONLY 44-byte INFO
// chunk in a real .mgn (a nested FORM PRIM's own INFO chunk is 4 bytes, a
// different, unrelated field). Searched by tag+size together, same
// disambiguation approach StaticMesh.cpp already uses for its VTXA INFO.
const IffChunk* findMeshInfo(const IffChunk& chunk) {
    if (chunk.id != kFormTag && chunk.id == kInfoTag && chunk.data.size() == 44) {
        return &chunk;
    }
    for (const auto& child : chunk.children) {
        if (const IffChunk* found = findMeshInfo(child)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

SkeletalMeshData SkeletalMesh::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kSkmgTag) {
        throw std::runtime_error("SkeletalMesh::parse: not a FORM(SKMG)-rooted file");
    }
    const IffChunk& root = topLevel[0];

    const IffChunk* info = findMeshInfo(root);
    if (info == nullptr) {
        throw std::runtime_error("SkeletalMesh::parse: mesh INFO chunk not found");
    }
    soe::PacketBuffer infoBuf = info->data;
    infoBuf.resetReadCursor();
    infoBuf.readUint32(); // unused
    infoBuf.readUint32(); // unused
    uint32_t numSkeletons = infoBuf.readUint32();
    uint32_t numBones = infoBuf.readUint32();
    uint32_t numPoints = infoBuf.readUint32();
    infoBuf.readUint32(); // numTwdt - derived instead from TWHD's own per-vertex counts below
    uint32_t numNorm = infoBuf.readUint32();
    uint32_t numPSDT = infoBuf.readUint32();
    // numBLT + 4 trailing int16 fields: unused, not read.

    const IffChunk* sktmChunk = findFirstChunk(root, kSktmTag);
    if (sktmChunk == nullptr || numSkeletons == 0) {
        throw std::runtime_error("SkeletalMesh::parse: SKTM (skeleton reference) chunk not found");
    }
    soe::PacketBuffer sktmBuf = sktmChunk->data;
    sktmBuf.resetReadCursor();
    std::string skeletonFilename = readNulTerminatedString(sktmBuf);

    const IffChunk* xfnmChunk = findFirstChunk(root, kXfnmTag);
    if (xfnmChunk == nullptr) {
        throw std::runtime_error("SkeletalMesh::parse: XFNM (bone names) chunk not found");
    }
    soe::PacketBuffer xfnmBuf = xfnmChunk->data;
    xfnmBuf.resetReadCursor();
    std::vector<std::string> boneNames;
    boneNames.reserve(numBones);
    for (uint32_t i = 0; i < numBones; ++i) {
        boneNames.push_back(readNulTerminatedString(xfnmBuf));
    }

    const IffChunk* posnChunk = findFirstChunk(root, kPosnTag);
    if (posnChunk == nullptr) {
        throw std::runtime_error("SkeletalMesh::parse: POSN (positions) chunk not found");
    }
    soe::PacketBuffer posnBuf = posnChunk->data;
    posnBuf.resetReadCursor();
    std::vector<Float3> meshPositions;
    meshPositions.reserve(numPoints);
    for (uint32_t i = 0; i < numPoints; ++i) {
        Float3 p;
        p.x = posnBuf.readFloat();
        p.y = posnBuf.readFloat();
        p.z = posnBuf.readFloat();
        meshPositions.push_back(p);
    }

    const IffChunk* normChunk = findFirstChunk(root, kNormTag);
    if (normChunk == nullptr) {
        throw std::runtime_error("SkeletalMesh::parse: NORM (normals) chunk not found");
    }
    soe::PacketBuffer normBuf = normChunk->data;
    normBuf.resetReadCursor();
    std::vector<Float3> meshNormals;
    meshNormals.reserve(numNorm);
    for (uint32_t i = 0; i < numNorm; ++i) {
        Float3 n;
        n.x = normBuf.readFloat();
        n.y = normBuf.readFloat();
        n.z = normBuf.readFloat();
        meshNormals.push_back(n);
    }

    const IffChunk* twhdChunk = findFirstChunk(root, kTwhdTag);
    const IffChunk* twdtChunk = findFirstChunk(root, kTwdtTag);
    if (twhdChunk == nullptr || twdtChunk == nullptr) {
        throw std::runtime_error("SkeletalMesh::parse: TWHD/TWDT (bone weight) chunks not found");
    }
    soe::PacketBuffer twhdBuf = twhdChunk->data;
    twhdBuf.resetReadCursor();
    std::vector<uint32_t> weightCounts;
    weightCounts.reserve(numPoints);
    for (uint32_t i = 0; i < numPoints; ++i) {
        weightCounts.push_back(twhdBuf.readUint32());
    }
    soe::PacketBuffer twdtBuf = twdtChunk->data;
    twdtBuf.resetReadCursor();
    std::vector<std::vector<BoneWeight>> vertexWeights;
    vertexWeights.resize(numPoints);
    for (uint32_t i = 0; i < numPoints; ++i) {
        vertexWeights[i].reserve(weightCounts[i]);
        for (uint32_t w = 0; w < weightCounts[i]; ++w) {
            BoneWeight bw;
            bw.boneIndex = twdtBuf.readUint32();
            bw.weight = twdtBuf.readFloat();
            vertexWeights[i].push_back(bw);
        }
    }

    // Zero or more per-shader submesh blocks. A real mesh can legitimately
    // have zero (a confirmed anomaly, e.g. a placeholder/unused creature
    // asset) - that's not a parse error, just an empty `submeshes` list.
    std::vector<const IffChunk*> psdtForms = findAllForms(root, kPsdtFormTag);
    std::vector<SkeletalMeshSubmesh> submeshes;
    submeshes.reserve(psdtForms.size());
    for (const IffChunk* psdt : psdtForms) {
        const IffChunk* nameChunk = findFirstChunk(*psdt, kNameTag);
        const IffChunk* pidxChunk = findFirstChunk(*psdt, kPidxTag);
        const IffChunk* nidxChunk = findFirstChunk(*psdt, kNidxTag);
        const IffChunk* tcsfForm = findFirstForm(*psdt, kTcsfFormTag);
        const IffChunk* primForm = findFirstForm(*psdt, kPrimFormTag);
        if (nameChunk == nullptr || pidxChunk == nullptr || nidxChunk == nullptr ||
            tcsfForm == nullptr || primForm == nullptr) {
            throw std::runtime_error("SkeletalMesh::parse: PSDT submesh missing a required chunk");
        }
        const IffChunk* tcsdChunk = findFirstChunk(*tcsfForm, kTcsdTag);
        const IffChunk* itlChunk = findFirstChunk(*primForm, kItlTag);
        const IffChunk* oitlChunk = itlChunk == nullptr ? findFirstChunk(*primForm, kOitlTag) : nullptr;
        if (tcsdChunk == nullptr || (itlChunk == nullptr && oitlChunk == nullptr)) {
            throw std::runtime_error("SkeletalMesh::parse: PSDT submesh missing TCSD/ITL/OITL");
        }

        SkeletalMeshSubmesh submesh;
        soe::PacketBuffer nameBuf = nameChunk->data;
        nameBuf.resetReadCursor();
        submesh.shaderFilename = readNulTerminatedString(nameBuf);

        // PIDX carries its own leading count; NIDX and TCSD do NOT - both
        // reuse PIDX's count directly (confirmed against real byte sizes:
        // NIDX's size is exactly count*4 with no +4 header, TCSD's is
        // exactly count*8 - matches meshLib's own readNIDX()/readTCSD(),
        // which reuse the `numIndex` set by the preceding readPIDX() call
        // rather than reading their own).
        soe::PacketBuffer pidxBuf = pidxChunk->data;
        pidxBuf.resetReadCursor();
        uint32_t numIndex = pidxBuf.readUint32();
        std::vector<uint32_t> pidx;
        pidx.reserve(numIndex);
        for (uint32_t i = 0; i < numIndex; ++i) {
            pidx.push_back(pidxBuf.readUint32());
        }

        soe::PacketBuffer nidxBuf = nidxChunk->data;
        nidxBuf.resetReadCursor();
        std::vector<uint32_t> nidx;
        nidx.reserve(numIndex);
        for (uint32_t i = 0; i < numIndex; ++i) {
            nidx.push_back(nidxBuf.readUint32());
        }

        soe::PacketBuffer tcsdBuf = tcsdChunk->data;
        tcsdBuf.resetReadCursor();
        submesh.positions.reserve(numIndex);
        submesh.normals.reserve(numIndex);
        submesh.uv0.reserve(numIndex);
        for (uint32_t i = 0; i < numIndex; ++i) {
            if (pidx[i] >= meshPositions.size() || nidx[i] >= meshNormals.size()) {
                throw std::runtime_error("SkeletalMesh::parse: PIDX/NIDX index out of range");
            }
            submesh.positions.push_back(meshPositions[pidx[i]]);
            submesh.normals.push_back(meshNormals[nidx[i]]);
            Float2 uv;
            uv.x = tcsdBuf.readFloat();
            uv.y = tcsdBuf.readFloat();
            submesh.uv0.push_back(uv);
        }

        if (itlChunk != nullptr) {
            soe::PacketBuffer itlBuf = itlChunk->data;
            itlBuf.resetReadCursor();
            uint32_t numTriangles = itlBuf.readUint32();
            submesh.indices.reserve(static_cast<size_t>(numTriangles) * 3);
            for (uint32_t i = 0; i < numTriangles; ++i) {
                submesh.indices.push_back(itlBuf.readUint32());
                submesh.indices.push_back(itlBuf.readUint32());
                submesh.indices.push_back(itlBuf.readUint32());
            }
        } else {
            // OITL: uint32 count, then per-triangle [int16 group, 3x
            // uint32 vertex index] - the group is discarded (see kOitlTag's
            // own comment), every triangle flattened into one combined
            // index list.
            soe::PacketBuffer oitlBuf = oitlChunk->data;
            oitlBuf.resetReadCursor();
            uint32_t numTriangles = oitlBuf.readUint32();
            submesh.indices.reserve(static_cast<size_t>(numTriangles) * 3);
            for (uint32_t i = 0; i < numTriangles; ++i) {
                oitlBuf.readUint16(); // group - not used
                submesh.indices.push_back(oitlBuf.readUint32());
                submesh.indices.push_back(oitlBuf.readUint32());
                submesh.indices.push_back(oitlBuf.readUint32());
            }
        }

        submeshes.push_back(std::move(submesh));
    }

    SkeletalMeshData result;
    result.skeletonFilename = std::move(skeletonFilename);
    result.boneNames = std::move(boneNames);
    result.vertexWeights = std::move(vertexWeights);
    result.submeshes = std::move(submeshes);
    return result;
}

} // namespace assets
