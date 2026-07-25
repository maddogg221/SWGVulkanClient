#include "assets/SkeletalAppearance.h"

#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kSmatTag = 0x534D4154; // 'SMAT'
constexpr uint32_t kInfoTag = 0x494E464F; // 'INFO'
constexpr uint32_t kMsgnTag = 0x4D53474E; // 'MSGN'
constexpr uint32_t kSktiTag = 0x534B5449; // 'SKTI'
constexpr uint32_t kLatxTag = 0x4C415458; // 'LATX'
} // namespace

SkeletalAppearanceData SkeletalAppearance::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kSmatTag) {
        throw std::runtime_error("SkeletalAppearance::parse: not a FORM(SMAT)-rooted file");
    }
    const IffChunk& root = topLevel[0];

    const IffChunk* infoChunk = findFirstChunk(root, kInfoTag);
    if (infoChunk == nullptr) {
        throw std::runtime_error("SkeletalAppearance::parse: INFO chunk not found");
    }
    soe::PacketBuffer infoBuf = infoChunk->data;
    infoBuf.resetReadCursor();
    uint32_t numMSGN = infoBuf.readUint32();
    uint16_t numSKTI = infoBuf.readUint16();

    const IffChunk* msgnChunk = findFirstChunk(root, kMsgnTag);
    if (msgnChunk == nullptr) {
        throw std::runtime_error("SkeletalAppearance::parse: MSGN chunk not found");
    }
    soe::PacketBuffer msgnBuf = msgnChunk->data;
    msgnBuf.resetReadCursor();
    std::vector<std::string> meshFilenames;
    meshFilenames.reserve(numMSGN);
    for (uint32_t i = 0; i < numMSGN; ++i) {
        meshFilenames.push_back(readNulTerminatedString(msgnBuf));
    }

    const IffChunk* sktiChunk = findFirstChunk(root, kSktiTag);
    if (sktiChunk == nullptr || numSKTI == 0) {
        throw std::runtime_error("SkeletalAppearance::parse: SKTI (skeleton reference) chunk not found");
    }
    soe::PacketBuffer sktiBuf = sktiChunk->data;
    sktiBuf.resetReadCursor();
    // Only the first [skeletonFile, name] pair's filename is used - real
    // data checked this session always has numSKTI == 1.
    std::string skeletonFilename = readNulTerminatedString(sktiBuf);

    std::string latFilename;
    if (const IffChunk* latxChunk = findFirstChunk(root, kLatxTag)) {
        soe::PacketBuffer latxBuf = latxChunk->data;
        latxBuf.resetReadCursor();
        uint16_t numLatxPairs = latxBuf.readUint16();
        if (numLatxPairs > 0) {
            // Only the first [skeletonFile, latFile] pair is needed, same
            // reasoning as SKTI above.
            readNulTerminatedString(latxBuf); // skeletonFile, unused (already have it from SKTI)
            latFilename = readNulTerminatedString(latxBuf);
        }
    }

    SkeletalAppearanceData result;
    result.meshFilenames = std::move(meshFilenames);
    result.skeletonFilename = std::move(skeletonFilename);
    result.latFilename = std::move(latFilename);
    return result;
}

} // namespace assets
