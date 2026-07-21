#include "assets/AppearanceTemplate.h"

#include <functional>
#include <optional>
#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kNameTag = 0x4E414D45; // 'NAME'
}

AppearanceTemplateData AppearanceTemplate::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag) {
        throw std::runtime_error("AppearanceTemplate::parse: not a FORM-rooted file");
    }

    std::optional<std::string> referenced;
    std::function<void(const IffChunk&)> visit = [&](const IffChunk& chunk) {
        if (referenced.has_value()) {
            return;
        }
        if (chunk.id == kFormTag) {
            for (const auto& child : chunk.children) {
                visit(child);
                if (referenced.has_value()) {
                    return;
                }
            }
        } else if (chunk.id == kNameTag) {
            soe::PacketBuffer data = chunk.data;
            data.resetReadCursor();
            referenced = readNulTerminatedString(data);
        }
    };
    for (const auto& top : topLevel) {
        visit(top);
        if (referenced.has_value()) {
            break;
        }
    }

    if (!referenced.has_value()) {
        throw std::runtime_error("AppearanceTemplate::parse: NAME chunk not found");
    }

    AppearanceTemplateData result;
    result.referencedFilename = *referenced;
    return result;
}

} // namespace assets
