#include "assets/ShaderTemplate.h"

#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {

constexpr uint32_t kSshtTag = 0x53534854; // 'SSHT'
constexpr uint32_t kCshdTag = 0x43534844; // 'CSHD'
constexpr uint32_t kTxmsTag = 0x54584D53; // 'TXMS'
constexpr uint32_t kTxmTag = 0x54584D20;  // 'TXM ' (trailing space)
constexpr uint32_t kDataTag = 0x44415441; // 'DATA'
constexpr uint32_t kNameTag = 0x4E414D45; // 'NAME'

// Reads every real texture slot out of a real FORM TXMS (one real FORM
// "TXM " per slot - see ShaderTemplate.h's own comment for the exact
// nested shape). Only the "MAIN" slot is kept; every other real slot tag
// this project has seen (SPEC/NRML/CNRM/ENVM/EMIS/DETA/HUEB) is read and
// discarded, matching this pass' explicit diffuse-texture-only scope.
void readTxmsInto(const IffChunk& txms, ShaderTemplateData& result) {
    for (const IffChunk& txm : txms.children) {
        if (txm.id != kFormTag || txm.formType != kTxmTag) {
            continue;
        }
        const IffChunk* dataChunk = findFirstChunk(txm, kDataTag);
        const IffChunk* nameChunk = findFirstChunk(txm, kNameTag);
        if (dataChunk == nullptr || nameChunk == nullptr) {
            continue; // malformed slot - skip rather than fail the whole shader
        }

        // The real slot tag is stored byte-reversed on disk - confirmed
        // against real bytes (a real MAIN slot's DATA chunk starts with the
        // literal bytes "NIAM", not "MAIN"); a real DATA chunk is 11 bytes
        // total (4-byte reversed tag + 7 more bytes this project doesn't
        // read, e.g. UV set/wrap-mode fields), only the first 4 matter here.
        soe::PacketBuffer dataBuf = dataChunk->data;
        dataBuf.resetReadCursor();
        std::vector<uint8_t> tagBytes = dataBuf.readBytes(4);
        std::string tag(tagBytes.rbegin(), tagBytes.rend());

        soe::PacketBuffer nameBuf = nameChunk->data;
        nameBuf.resetReadCursor();
        std::string texturePath = readNulTerminatedString(nameBuf);

        if (tag == "MAIN") {
            result.mainTextureFilename = std::move(texturePath);
        }
    }
}

// Parses the common body shared by both real root shapes (SSHT itself, and
// CSHD's own inner FORM SSHT) - real texture slots always live here
// regardless of which root wraps them.
void parseSshtBody(const IffChunk& sshtForm, ShaderTemplateData& result) {
    const IffChunk* txms = findFirstForm(sshtForm, kTxmsTag);
    if (txms != nullptr) {
        readTxmsInto(*txms, result);
    }
}

} // namespace

ShaderTemplateData ShaderTemplate::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag) {
        throw std::runtime_error("ShaderTemplate::parse: not a FORM-rooted file");
    }
    const IffChunk& root = topLevel[0];

    ShaderTemplateData result;
    if (root.formType == kSshtTag) {
        parseSshtBody(root, result);
    } else if (root.formType == kCshdTag) {
        // A real CSHD wraps its own inner FORM SSHT one level down (a real
        // customizable/dyeable shader) - the palette-recoloring block
        // alongside it (FORM TFAC) is deliberately not read, out of scope
        // for diffuse-texture-only resolution.
        const IffChunk* innerSsht = findFirstForm(root, kSshtTag);
        if (innerSsht != nullptr) {
            parseSshtBody(*innerSsht, result);
        }
    } else {
        throw std::runtime_error("ShaderTemplate::parse: unsupported root FORM type");
    }

    return result;
}

} // namespace assets
