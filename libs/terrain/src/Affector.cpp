#include "terrain/Affector.h"

#include <stdexcept>

#include "GeneratorItem.h"
#include "Tags.h"

namespace terrain {

namespace {

// -- HeightData (shared sub-form for AffectorRoad/AffectorRiver) --

HeightDataSegment readPackedVector3Points(soe::PacketBuffer buf) {
    buf.resetReadCursor();
    HeightDataSegment seg;
    while (buf.remaining() >= 12) {
        assets::Float3 p;
        p.x = buf.readFloat();
        p.y = buf.readFloat();
        p.z = buf.readFloat();
        seg.points.push_back(p);
    }
    return seg;
}

// `roadOrHdtaForm` is the located `FORM ROAD` or `FORM HDTA` node itself.
HeightData parseHeightData(const assets::IffChunk& roadOrHdtaForm) {
    HeightData result;
    if (roadOrHdtaForm.children.empty()) {
        return result;
    }
    const assets::IffChunk& first = roadOrHdtaForm.children[0];
    if (first.id == versionTag(0)) {
        // load_0000: a single implicit segment, stored as a plain CHNK
        // tagged "0000" (not a version FORM - HeightData's on-disk shape
        // genuinely differs between its two versions).
        result.push_back(readPackedVector3Points(first.data));
    } else if (first.id == assets::kFormTag && first.formType == versionTag(1)) {
        for (const auto& child : first.children) {
            if (child.id == kTagSGMT) {
                result.push_back(readPackedVector3Points(child.data));
            }
        }
    } else {
        throw std::runtime_error("terrain: HeightData unrecognized version");
    }
    return result;
}

// AffectorRoad/AffectorRiver share this exact shape: FORM DATA containing
// an optional HeightData sub-form (FORM ROAD or FORM HDTA - either tag may
// appear, real data doesn't confirm which) followed by a sibling CHNK DATA
// with the affector's own fields.
struct RoadRiverPayload {
    LayerItemHeader header;
    HeightData heightData;
    soe::PacketBuffer data;
};

RoadRiverPayload enterRoadRiverVersioned(const assets::IffChunk& form, uint32_t expectedVersion,
                                          const char* typeName) {
    if (form.children.empty() || form.children[0].id != assets::kFormTag) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing version FORM");
    }
    const assets::IffChunk& versionForm = form.children[0];
    if (versionForm.formType != expectedVersion) {
        throw std::runtime_error(std::string("terrain: ") + typeName +
                                  " unsupported version (real files only use one)");
    }
    if (versionForm.children.empty() || versionForm.children[0].id != assets::kFormTag ||
        versionForm.children[0].formType != kTagIHDR) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing IHDR");
    }

    RoadRiverPayload result;
    result.header = parseLayerItemHeader(versionForm.children[0]);

    const assets::IffChunk* dataForm = findChildForm(versionForm, kTagDATA);
    if (dataForm == nullptr) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing FORM DATA");
    }

    const assets::IffChunk* heightForm = findChildForm(*dataForm, kTagROAD);
    if (heightForm == nullptr) {
        heightForm = findChildForm(*dataForm, kTagHDTA);
    }
    if (heightForm != nullptr) {
        result.heightData = parseHeightData(*heightForm);
    }

    const assets::IffChunk* dataChunk = dataForm->findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing CHNK DATA");
    }
    result.data = dataChunk->data;
    result.data.resetReadCursor();
    return result;
}

// -- concrete affector parsers --

Affector parseAffectorEnvironment(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "AffectorEnvironment");
    AffectorEnvironment a;
    a.familyId = static_cast<int>(v.data.readUint32());
    a.useFeatherClampOverride = v.data.readUint32() != 0;
    a.featherClampOverride = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorHeightConstant(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "AffectorHeightConstant");
    AffectorHeightConstant a;
    a.operation = static_cast<int>(v.data.readUint32());
    a.height = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorHeightFractal(const assets::IffChunk& form) {
    VersionedPayload v = enterVersionedNestedParm(form, versionTag(3), "AffectorHeightFractal");
    AffectorHeightFractal a;
    a.familyId = static_cast<int>(v.data.readUint32());
    a.operation = static_cast<int>(v.data.readUint32());
    a.scaleY = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorHeightTerrace(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(4), "AffectorHeightTerrace");
    AffectorHeightTerrace a;
    a.fraction = v.data.readFloat();
    a.height = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorColorConstant(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "AffectorColorConstant");
    AffectorColorConstant a;
    a.operation = static_cast<int>(v.data.readUint32());
    a.colorR = v.data.readByte();
    a.colorG = v.data.readByte();
    a.colorB = v.data.readByte();
    return Affector{v.header, a};
}

Affector parseAffectorColorRampHeight(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "AffectorColorRampHeight");
    AffectorColorRampHeight a;
    a.operation = static_cast<int>(v.data.readUint32());
    a.lowHeight = v.data.readFloat();
    a.highHeight = v.data.readFloat();
    a.imageName = assets::readNulTerminatedString(v.data);
    return Affector{v.header, a};
}

Affector parseAffectorColorRampFractal(const assets::IffChunk& form) {
    VersionedPayload v = enterVersionedNestedParm(form, versionTag(1), "AffectorColorRampFractal");
    AffectorColorRampFractal a;
    a.familyId = static_cast<int>(v.data.readUint32());
    a.operation = static_cast<int>(v.data.readUint32());
    a.imageName = assets::readNulTerminatedString(v.data);
    return Affector{v.header, a};
}

Affector parseAffectorShaderConstant(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(1), "AffectorShaderConstant");
    AffectorShaderConstant a;
    a.familyId = static_cast<int>(v.data.readUint32());
    a.useFeatherClampOverride = v.data.readUint32() != 0;
    a.featherClampOverride = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorShaderReplace(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(1), "AffectorShaderReplace");
    AffectorShaderReplace a;
    a.sourceFamilyId = static_cast<int>(v.data.readUint32());
    a.destinationFamilyId = static_cast<int>(v.data.readUint32());
    a.useFeatherClampOverride = v.data.readUint32() != 0;
    a.featherClampOverride = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorFloraStatic(const assets::IffChunk& form, bool collidable) {
    VersionedPayload v = enterVersioned(form, versionTag(4), "AffectorFloraStatic");
    AffectorFloraStatic a;
    a.familyId = static_cast<int>(v.data.readUint32());
    a.operation = static_cast<int>(v.data.readUint32());
    a.removeAll = v.data.readUint32() != 0;
    a.densityOverride = v.data.readUint32() != 0;
    a.densityOverrideDensity = v.data.readFloat();
    a.collidable = collidable;
    return Affector{v.header, a};
}

Affector parseAffectorFloraDynamic(const assets::IffChunk& form, bool isNear) {
    VersionedPayload v = enterVersioned(form, versionTag(2), "AffectorFloraDynamic");
    AffectorFloraDynamic a;
    a.familyId = static_cast<int>(v.data.readUint32());
    a.operation = static_cast<int>(v.data.readUint32());
    a.removeAll = v.data.readUint32() != 0;
    a.densityOverride = v.data.readUint32() != 0;
    a.densityOverrideDensity = v.data.readFloat();
    a.near_ = isNear;
    return Affector{v.header, a};
}

Affector parseAffectorExclude(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "AffectorExclude");
    return Affector{v.header, AffectorExclude{}};
}

Affector parseAffectorPassable(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "AffectorPassable");
    AffectorPassable a;
    a.passable = v.data.readByte() != 0;
    a.featherThreshold = v.data.readFloat();
    return Affector{v.header, a};
}

Affector parseAffectorRoad(const assets::IffChunk& form) {
    RoadRiverPayload v = enterRoadRiverVersioned(form, versionTag(5), "AffectorRoad");
    AffectorRoad a;
    a.heightData = std::move(v.heightData);
    a.points = readPointList2D(v.data);
    a.width = v.data.readFloat();
    a.familyId = static_cast<int>(v.data.readUint32());
    a.featherFunction = static_cast<int>(v.data.readUint32());
    a.featherDistance = readClampedFeatherDistance(v.data);
    a.featherFunctionShader = static_cast<int>(v.data.readUint32());
    a.featherDistanceShader = readClampedFeatherDistance(v.data);
    return Affector{v.header, a};
}

Affector parseAffectorRiver(const assets::IffChunk& form) {
    RoadRiverPayload v = enterRoadRiverVersioned(form, versionTag(5), "AffectorRiver");
    AffectorRiver a;
    a.heightData = std::move(v.heightData);
    a.points = readPointList2D(v.data);
    a.width = v.data.readFloat();
    a.bankFamilyId = static_cast<int>(v.data.readUint32());
    a.bottomFamilyId = static_cast<int>(v.data.readUint32());
    a.featherFunction = static_cast<int>(v.data.readUint32());
    a.featherDistance = readClampedFeatherDistance(v.data);
    a.trenchDepth = v.data.readFloat();
    a.velocity = v.data.readFloat();
    a.hasLocalWaterTable = v.data.readUint32() != 0;
    a.localWaterTableDepth = v.data.readFloat();
    a.localWaterTableWidth = v.data.readFloat();
    a.localWaterTableShaderSize = v.data.readFloat();
    a.localWaterTableShaderTemplateName = assets::readNulTerminatedString(v.data);
    return Affector{v.header, a};
}

} // namespace

bool isAffectorTag(uint32_t tag) {
    switch (tag) {
        case kTagAHSM:
        case kTagAHBM:
        case kTagACBM:
        case kTagASBM:
        case kTagAFBM:
        case kTagAENV:
        case kTagAHTR:
        case kTagAHCN:
        case kTagAHFR:
        case kTagACCN:
        case kTagACRH:
        case kTagACRF:
        case kTagASCN:
        case kTagASRP:
        case kTagAFCN:
        case kTagAFSC:
        case kTagAFSN:
        case kTagARCN:
        case kTagAFDN:
        case kTagAFDF:
        case kTagARIB:
        case kTagAEXC:
        case kTagAPAS:
        case kTagAROA:
        case kTagARIV:
            return true;
        default:
            return false;
    }
}

std::optional<Affector> parseAffector(const assets::IffChunk& form) {
    switch (form.formType) {
        case kTagAHSM:
        case kTagAHBM:
        case kTagACBM:
        case kTagASBM:
        case kTagAFBM:
            // Dead tags - no live object in the real engine either.
            return std::nullopt;
        case kTagAENV:
            return parseAffectorEnvironment(form);
        case kTagAHCN:
            return parseAffectorHeightConstant(form);
        case kTagAHFR:
            return parseAffectorHeightFractal(form);
        case kTagAHTR:
            return parseAffectorHeightTerrace(form);
        case kTagACCN:
            return parseAffectorColorConstant(form);
        case kTagACRH:
            return parseAffectorColorRampHeight(form);
        case kTagACRF:
            return parseAffectorColorRampFractal(form);
        case kTagASCN:
            return parseAffectorShaderConstant(form);
        case kTagASRP:
            return parseAffectorShaderReplace(form);
        case kTagAFCN:
        case kTagAFSC:
            return parseAffectorFloraStatic(form, /*collidable=*/true);
        case kTagAFSN:
            return parseAffectorFloraStatic(form, /*collidable=*/false);
        case kTagARCN:
        case kTagAFDN:
            return parseAffectorFloraDynamic(form, /*near=*/true);
        case kTagAFDF:
            return parseAffectorFloraDynamic(form, /*near=*/false);
        case kTagAEXC:
            return parseAffectorExclude(form);
        case kTagAPAS:
            return parseAffectorPassable(form);
        case kTagAROA:
            return parseAffectorRoad(form);
        case kTagARIV:
            return parseAffectorRiver(form);
        case kTagARIB:
            throw std::runtime_error(
                "terrain: AffectorRibbon is recognized but not implemented - never observed in "
                "any of the 10 classic-planet .trn files, see Affector.h");
        default:
            throw std::runtime_error("terrain: parseAffector called with a non-affector tag");
    }
}

} // namespace terrain
