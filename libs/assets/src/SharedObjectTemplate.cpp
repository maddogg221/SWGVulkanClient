#include "assets/SharedObjectTemplate.h"

#include <functional>
#include <optional>
#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

SharedObjectTemplateData SharedObjectTemplate::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag) {
        throw std::runtime_error("SharedObjectTemplate::parse: not a FORM-rooted file");
    }

    // Every field lives in a self-describing 'XXXX' leaf chunk (name then
    // value, both NUL-terminated strings for the fields this project reads -
    // see IffReader.h's kParamTag comment for how this was discovered).
    // Recursively searches every FORM rather than assuming a fixed nesting
    // depth/form-type-number - real templates showed a version-numbered
    // FORM (e.g. '0007') wrapping the actual parameters, and that number is
    // not guaranteed stable across every template.
    std::optional<std::string> appearanceFilename;
    std::optional<std::string> portalLayoutFilename;
    auto bothFound = [&] {
        return appearanceFilename.has_value() && portalLayoutFilename.has_value();
    };
    std::function<void(const IffChunk&)> visit = [&](const IffChunk& chunk) {
        if (bothFound()) {
            return;
        }
        if (chunk.id == kFormTag) {
            for (const auto& child : chunk.children) {
                visit(child);
                if (bothFound()) {
                    return;
                }
            }
        } else if (chunk.id == kParamTag) {
            // Not every XXXX leaf is a name+value string pair - real
            // creature/player templates (confirmed live against
            // shared_human_male.iff) also use this same tag for small,
            // differently-shaped fields immediately following each
            // PCCV/RICV customization-variable FORM (5 bytes, sometimes 0 -
            // exact meaning not chased down, not needed here). A generic
            // "read a NUL-terminated name first" attempt on one of those
            // throws (no NUL within the chunk's own short length) - caught
            // and skipped per-chunk here rather than aborting the whole
            // tree walk, since the field this function actually looks for
            // is a plain name+value chunk elsewhere in the same tree. Same
            // "dead tag, don't fail on it" precedent already used for
            // BLTS/HPTS/AHSM/AHBM elsewhere in this codebase, just applied
            // via catching instead of a tag-name whitelist (these chunks
            // have no distinguishing tag of their own - only trying to read
            // them reveals the shape mismatch).
            try {
                soe::PacketBuffer data = chunk.data; // copy: needs its own mutable read cursor
                data.resetReadCursor();
                std::string name = readNulTerminatedString(data);
                // One byte between the name and value strings - a type code
                // (this project only ever reads string-valued fields, so
                // its exact meaning for other types was never chased down;
                // confirmed present by real byte-count arithmetic: name(18)
                // + NUL(1) + this byte(1) + value(34) + NUL(1) == the real
                // chunk's exact 55-byte size). Same shape for every
                // string-valued field this project reads, not just
                // appearanceFilename.
                if (name == "appearanceFilename") {
                    data.readByte();
                    appearanceFilename = readNulTerminatedString(data);
                } else if (name == "portalLayoutFilename") {
                    data.readByte();
                    portalLayoutFilename = readNulTerminatedString(data);
                }
            } catch (const std::out_of_range&) {
                // Not a name+value pair - skip, keep walking.
            }
        }
    };
    for (const auto& top : topLevel) {
        visit(top);
        if (bothFound()) {
            break;
        }
    }

    if (!appearanceFilename.has_value() && !portalLayoutFilename.has_value()) {
        throw std::runtime_error(
            "SharedObjectTemplate::parse: neither appearanceFilename nor portalLayoutFilename found");
    }

    SharedObjectTemplateData result;
    result.appearanceFilename = appearanceFilename.value_or("");
    result.portalLayoutFilename = portalLayoutFilename.value_or("");
    return result;
}

} // namespace assets
