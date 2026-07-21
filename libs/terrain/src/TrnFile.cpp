#include "terrain/TrnFile.h"

#include <stdexcept>

namespace terrain {

namespace {

// IFF form-type tags are stored as ASCII, but IffChunk::formType is decoded
// as a big-endian uint32 by IffReader (matching every other chunk id/tag in
// this project) - build the same constants that comparison, not string
// compares against raw bytes.
constexpr uint32_t makeTag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(d));
}

constexpr uint32_t kTagPTAT = makeTag('P', 'T', 'A', 'T');
constexpr uint32_t kTagMPTA = makeTag('M', 'P', 'T', 'A');
constexpr uint32_t kTag0013 = makeTag('0', '0', '1', '3');
constexpr uint32_t kTag0014 = makeTag('0', '0', '1', '4');
constexpr uint32_t kTag0015 = makeTag('0', '0', '1', '5');
constexpr uint32_t kTagDATA = makeTag('D', 'A', 'T', 'A');
constexpr uint32_t kTagTGEN = makeTag('T', 'G', 'E', 'N');

TrnHeader::FloraPlacementParams parseFloraPlacementParams(soe::PacketBuffer& buf) {
    TrnHeader::FloraPlacementParams params;
    params.minimumDistance = buf.readFloat();
    params.maximumDistance = buf.readFloat();
    params.tileSize = buf.readFloat();
    params.tileBorder = buf.readFloat();
    params.seed = buf.readUint32();
    return params;
}

TrnHeader parseHeader(soe::PacketBuffer& buf, uint32_t version) {
    TrnHeader header;
    header.name = assets::readNulTerminatedString(buf);
    header.mapWidthInMeters = buf.readFloat();
    header.chunkWidthInMeters = buf.readFloat();
    header.numberOfTilesPerChunk = static_cast<int32_t>(buf.readUint32());
    header.useGlobalWaterTable = buf.readUint32() != 0;
    header.globalWaterTableHeight = buf.readFloat();
    header.globalWaterTableShaderSize = buf.readFloat();
    header.globalWaterTableShaderTemplateName = assets::readNulTerminatedString(buf);
    header.environmentCycleTime = buf.readFloat();

    if (version == kTag0013) {
        // 6 dummy legacy fields (4 strings + 2 float/string pairs + 1
        // int32), all discarded - PHASE_11_STATUS.md section 6. No real
        // .trn file on this machine uses version 0013 (naboo.trn is 0014),
        // so this branch is ported from source but not yet real-bytes
        // verified - flagged here rather than silently assumed correct.
        assets::readNulTerminatedString(buf);
        assets::readNulTerminatedString(buf);
        assets::readNulTerminatedString(buf);
        assets::readNulTerminatedString(buf);
        buf.readFloat();
        assets::readNulTerminatedString(buf);
        buf.readFloat();
        assets::readNulTerminatedString(buf);
        buf.readUint32();
    }

    header.collidable = parseFloraPlacementParams(buf);
    header.nonCollidable = parseFloraPlacementParams(buf);
    header.radial = parseFloraPlacementParams(buf);
    header.farRadial = parseFloraPlacementParams(buf);

    if (version == kTag0015) {
        header.legacyMap = buf.readByte() != 0;
    }

    return header;
}

} // namespace

TrnFile TrnFile::parse(const std::vector<uint8_t>& bytes) {
    TrnFile result;
    result.topLevel_ = assets::IffReader::parse(bytes);
    if (result.topLevel_.empty() || result.topLevel_[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: not an IFF FORM container");
    }
    const assets::IffChunk& root = result.topLevel_[0];
    if (root.formType != kTagPTAT && root.formType != kTagMPTA) {
        throw std::runtime_error("terrain: top-level FORM is not PTAT/MPTA");
    }
    if (root.children.empty() || root.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: missing version FORM");
    }
    const assets::IffChunk& versionForm = root.children[0];
    uint32_t version = versionForm.formType;
    if (version != kTag0013 && version != kTag0014 && version != kTag0015) {
        throw std::runtime_error("terrain: unrecognized version FORM");
    }

    const assets::IffChunk* dataChunk = versionForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error("terrain: missing CHNK DATA");
    }
    soe::PacketBuffer headerBuf = dataChunk->data;
    result.header_ = parseHeader(headerBuf, version);

    const assets::IffChunk* tgen = assets::findFirstForm(versionForm, kTagTGEN);
    if (tgen == nullptr) {
        throw std::runtime_error("terrain: missing FORM TGEN");
    }
    result.generatorForm_ = tgen;

    return result;
}

} // namespace terrain
