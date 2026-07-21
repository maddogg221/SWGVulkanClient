#include "assets/LodFile.h"

#include <functional>
#include <stdexcept>
#include <vector>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kChildTag = 0x43484C44; // 'CHLD'
}

LodFileData LodFile::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag) {
        throw std::runtime_error("LodFile::parse: not a FORM-rooted file");
    }

    std::vector<std::string> meshPaths;
    std::function<void(const IffChunk&)> visit = [&](const IffChunk& chunk) {
        if (chunk.id == kFormTag) {
            for (const auto& child : chunk.children) {
                visit(child);
            }
        } else if (chunk.id == kChildTag) {
            soe::PacketBuffer data = chunk.data;
            data.resetReadCursor();
            data.readFloat(); // distance threshold - not used by this project
            meshPaths.push_back(readNulTerminatedString(data));
        }
    };
    for (const auto& top : topLevel) {
        visit(top);
    }

    if (meshPaths.empty()) {
        throw std::runtime_error("LodFile::parse: no CHLD chunks found");
    }

    LodFileData result;
    result.highestDetailMeshFilename = meshPaths.front();
    for (const auto& path : meshPaths) {
        if (path.size() >= 7 && path.compare(path.size() - 7, 7, "_l0.msh") == 0) {
            result.highestDetailMeshFilename = path;
            break;
        }
    }
    return result;
}

} // namespace assets
