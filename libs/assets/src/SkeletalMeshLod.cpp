#include "assets/SkeletalMeshLod.h"

#include <functional>
#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kMlodTag = 0x4D4C4F44; // 'MLOD'
constexpr uint32_t kNameTag = 0x4E414D45; // 'NAME'
} // namespace

SkeletalMeshLodData SkeletalMeshLod::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kMlodTag) {
        throw std::runtime_error("SkeletalMeshLod::parse: not a FORM(MLOD)-rooted file");
    }

    std::vector<std::string> meshPaths;
    std::function<void(const IffChunk&)> visit = [&](const IffChunk& chunk) {
        if (chunk.id == kFormTag) {
            for (const auto& child : chunk.children) {
                visit(child);
            }
        } else if (chunk.id == kNameTag) {
            soe::PacketBuffer data = chunk.data;
            data.resetReadCursor();
            meshPaths.push_back(readNulTerminatedString(data));
        }
    };
    visit(topLevel[0]);

    if (meshPaths.empty()) {
        throw std::runtime_error("SkeletalMeshLod::parse: no NAME chunks found");
    }

    SkeletalMeshLodData result;
    result.highestDetailMeshFilename = meshPaths.front();
    for (const auto& path : meshPaths) {
        if (path.size() >= 7 && path.compare(path.size() - 7, 7, "_l0.mgn") == 0) {
            result.highestDetailMeshFilename = path;
            break;
        }
    }
    return result;
}

} // namespace assets
