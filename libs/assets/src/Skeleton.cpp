#include "assets/Skeleton.h"

#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {

constexpr uint32_t kSlodTag = 0x534C4F44; // 'SLOD'
constexpr uint32_t kSktmFormTag = 0x534B544D; // 'SKTM' (as a FORM subtype)
constexpr uint32_t kNameTag = 0x4E414D45; // 'NAME'
constexpr uint32_t kPrntTag = 0x50524E54; // 'PRNT'
constexpr uint32_t kRpreTag = 0x52505245; // 'RPRE'
constexpr uint32_t kRpstTag = 0x52505354; // 'RPST'
constexpr uint32_t kBptrTag = 0x42505452; // 'BPTR'
// 'BPRO' - per-bone "bind pose rotation" quaternion, same
// insertChunkFloatQuaternion() encoding as RPRE/RPST (confirmed directly
// against the leaked original SkeletonTemplateWriter.cpp source).
// The real client composes a bone's bind-pose-relative local rotation as
// `postRotation * (bindPoseRotation * preRotation)` (three terms), not the
// `preRotation * postRotation` two-term approximation this project used
// before discovering this chunk - every real `.skt` file has always
// carried this data; it was simply never read.
constexpr uint32_t kBproTag = 0x4250524F; // 'BPRO'

} // namespace

SkeletonData Skeleton::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kSlodTag) {
        throw std::runtime_error("Skeleton::parse: not a FORM(SLOD)-rooted file");
    }

    std::vector<const IffChunk*> sktmBlocks = findAllForms(topLevel[0], kSktmFormTag);
    if (sktmBlocks.empty()) {
        throw std::runtime_error("Skeleton::parse: no FORM SKTM block found");
    }

    // Real data confirms the highest-detail (most bones) block is listed
    // first, but selecting explicitly by bone count (via PRNT's size, one
    // int32 per bone) is more robust than relying on that ordering.
    const IffChunk* best = nullptr;
    size_t bestBoneCount = 0;
    for (const IffChunk* block : sktmBlocks) {
        const IffChunk* prnt = findFirstChunk(*block, kPrntTag);
        size_t boneCount = prnt != nullptr ? prnt->data.size() / 4 : 0;
        if (boneCount > bestBoneCount) {
            bestBoneCount = boneCount;
            best = block;
        }
    }
    if (best == nullptr) {
        throw std::runtime_error("Skeleton::parse: no usable FORM SKTM block (missing PRNT)");
    }

    const IffChunk* nameChunk = findFirstChunk(*best, kNameTag);
    const IffChunk* prntChunk = findFirstChunk(*best, kPrntTag);
    const IffChunk* rpreChunk = findFirstChunk(*best, kRpreTag);
    const IffChunk* rpstChunk = findFirstChunk(*best, kRpstTag);
    const IffChunk* bptrChunk = findFirstChunk(*best, kBptrTag);
    const IffChunk* bproChunk = findFirstChunk(*best, kBproTag);
    if (nameChunk == nullptr || prntChunk == nullptr || rpreChunk == nullptr ||
        rpstChunk == nullptr || bptrChunk == nullptr || bproChunk == nullptr) {
        throw std::runtime_error("Skeleton::parse: missing a required bone-data chunk");
    }

    size_t boneCount = bestBoneCount;

    soe::PacketBuffer nameBuf = nameChunk->data;
    nameBuf.resetReadCursor();
    std::vector<std::string> names;
    names.reserve(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        names.push_back(readNulTerminatedString(nameBuf));
    }

    soe::PacketBuffer prntBuf = prntChunk->data;
    prntBuf.resetReadCursor();
    soe::PacketBuffer rpreBuf = rpreChunk->data;
    rpreBuf.resetReadCursor();
    soe::PacketBuffer rpstBuf = rpstChunk->data;
    rpstBuf.resetReadCursor();
    soe::PacketBuffer bptrBuf = bptrChunk->data;
    bptrBuf.resetReadCursor();
    soe::PacketBuffer bproBuf = bproChunk->data;
    bproBuf.resetReadCursor();

    SkeletonData result;
    result.bones.reserve(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        SkeletonBone bone;
        bone.name = names[i];
        bone.parentIndex = static_cast<int32_t>(prntBuf.readUint32());
        // Real write order (confirmed directly against the leaked original
        // Iff::insertChunkFloatQuaternion source, 2026-07-25): scalar (w)
        // FIRST, then x,y,z - NOT x,y,z,w. This project's own existing
        // unit-length sanity test never caught the previous x,y,z,w
        // reading being wrong since magnitude is order-invariant; the real
        // writer source is unambiguous. `bindTranslation` is a plain
        // Vector (insertChunkFloatVector: x,y,z, no w) and is unaffected.
        bone.preRotation.w = rpreBuf.readFloat();
        bone.preRotation.x = rpreBuf.readFloat();
        bone.preRotation.y = rpreBuf.readFloat();
        bone.preRotation.z = rpreBuf.readFloat();
        bone.postRotation.w = rpstBuf.readFloat();
        bone.postRotation.x = rpstBuf.readFloat();
        bone.postRotation.y = rpstBuf.readFloat();
        bone.postRotation.z = rpstBuf.readFloat();
        bone.bindTranslation.x = bptrBuf.readFloat();
        bone.bindTranslation.y = bptrBuf.readFloat();
        bone.bindTranslation.z = bptrBuf.readFloat();
        bone.bindPoseRotation.w = bproBuf.readFloat();
        bone.bindPoseRotation.x = bproBuf.readFloat();
        bone.bindPoseRotation.y = bproBuf.readFloat();
        bone.bindPoseRotation.z = bproBuf.readFloat();
        result.bones.push_back(bone);
    }
    return result;
}

} // namespace assets
