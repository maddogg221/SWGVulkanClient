#include "terrain/Family.h"

#include <stdexcept>

#include "GeneratorItem.h"
#include "Tags.h"

namespace terrain {

namespace {

// -- MultiFractal (FORM MFRC), embedded by FractalGroup families --

FractalCombinationRule combinationRuleFromWire(uint32_t value) {
    switch (value) {
        case 0:
            return FractalCombinationRule::Add;
        case 1:
            return FractalCombinationRule::Multiply;
        case 2:
            return FractalCombinationRule::Crest;
        case 3:
            return FractalCombinationRule::Turbulence;
        case 4:
            return FractalCombinationRule::CrestClamp;
        case 5:
            return FractalCombinationRule::TurbulenceClamp;
        default:
            throw std::runtime_error("terrain: unrecognized fractal combination rule");
    }
}

// Mirrors MultiFractalReaderWriter::load (recovered separately alongside
// MultiFractal itself - see MultiFractal.cpp's provenance comment).
MultiFractalParams parseMultiFractalForm(const assets::IffChunk& mfrcForm) {
    if (mfrcForm.children.empty() || mfrcForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: MultiFractal missing version FORM");
    }
    const assets::IffChunk& versionForm = mfrcForm.children[0];
    const assets::IffChunk* dataChunk = versionForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error("terrain: MultiFractal missing CHNK DATA");
    }
    soe::PacketBuffer buf = dataChunk->data;
    buf.resetReadCursor();

    MultiFractalParams p;
    p.seed = buf.readUint32();
    p.useBias = buf.readUint32() != 0;
    p.bias = buf.readFloat();
    p.useGain = buf.readUint32() != 0;
    p.gain = buf.readFloat();
    p.numberOfOctaves = static_cast<int>(buf.readUint32());
    p.frequency = buf.readFloat();
    p.amplitude = buf.readFloat();
    p.scaleX = buf.readFloat();
    p.scaleY = buf.readFloat();
    if (versionForm.formType == versionTag(1)) {
        p.offsetX = buf.readFloat();
        p.offsetY = buf.readFloat();
    } else if (versionForm.formType != versionTag(0)) {
        throw std::runtime_error("terrain: MultiFractal unrecognized version");
    }
    p.combinationRule = combinationRuleFromWire(buf.readUint32());
    return p;
}

// -- ShaderGroup (FORM SGRP) --

ShaderFamily parseShaderFamily(soe::PacketBuffer& buf) {
    ShaderFamily f;
    f.familyId = static_cast<int>(buf.readUint32());
    f.name = assets::readNulTerminatedString(buf);
    f.surfacePropertiesName = assets::readNulTerminatedString(buf);
    f.colorR = buf.readByte();
    f.colorG = buf.readByte();
    f.colorB = buf.readByte();
    f.shaderSize = buf.readFloat();
    f.featherClamp = buf.readFloat();
    uint32_t n = buf.readUint32();
    f.children.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        ShaderFamilyChild c;
        c.shaderTemplateName = assets::readNulTerminatedString(buf);
        c.weight = buf.readFloat();
        f.children.push_back(c);
    }
    return f;
}

std::vector<ShaderFamily> parseShaderGroupForm(const assets::IffChunk& groupForm) {
    if (groupForm.children.empty() || groupForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: ShaderGroup missing version FORM");
    }
    const assets::IffChunk& versionForm = groupForm.children[0];
    if (versionForm.formType != versionTag(6)) {
        throw std::runtime_error("terrain: ShaderGroup unsupported version");
    }
    std::vector<ShaderFamily> families;
    for (const auto& child : versionForm.children) {
        if (child.id != kTagSFAM) {
            throw std::runtime_error("terrain: ShaderGroup: unexpected child");
        }
        soe::PacketBuffer buf = child.data;
        buf.resetReadCursor();
        families.push_back(parseShaderFamily(buf));
    }
    return families;
}

// -- FloraGroup (FORM FGRP) --

FloraFamily parseFloraFamily(soe::PacketBuffer& buf) {
    FloraFamily f;
    f.familyId = static_cast<int>(buf.readUint32());
    f.name = assets::readNulTerminatedString(buf);
    f.colorR = buf.readByte();
    f.colorG = buf.readByte();
    f.colorB = buf.readByte();
    f.density = buf.readFloat();
    f.floats = buf.readUint32() != 0;
    uint32_t n = buf.readUint32();
    f.children.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        FloraFamilyChild c;
        c.appearanceTemplateName = assets::readNulTerminatedString(buf);
        c.weight = buf.readFloat();
        c.shouldSway = buf.readUint32() != 0;
        c.displacement = buf.readFloat();
        c.period = buf.readFloat();
        c.alignToTerrain = buf.readUint32() != 0;
        c.shouldScale = buf.readUint32() != 0;
        c.minimumScale = buf.readFloat();
        c.maximumScale = buf.readFloat();
        f.children.push_back(c);
    }
    return f;
}

std::vector<FloraFamily> parseFloraGroupForm(const assets::IffChunk& groupForm) {
    if (groupForm.children.empty() || groupForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: FloraGroup missing version FORM");
    }
    const assets::IffChunk& versionForm = groupForm.children[0];
    if (versionForm.formType != versionTag(8)) {
        throw std::runtime_error("terrain: FloraGroup unsupported version");
    }
    std::vector<FloraFamily> families;
    for (const auto& child : versionForm.children) {
        if (child.id != kTagFFAM) {
            throw std::runtime_error("terrain: FloraGroup: unexpected child");
        }
        soe::PacketBuffer buf = child.data;
        buf.resetReadCursor();
        families.push_back(parseFloraFamily(buf));
    }
    return families;
}

// -- RadialGroup (FORM RGRP) --

RadialFamily parseRadialFamily(soe::PacketBuffer& buf) {
    RadialFamily f;
    f.familyId = static_cast<int>(buf.readUint32());
    f.name = assets::readNulTerminatedString(buf);
    f.colorR = buf.readByte();
    f.colorG = buf.readByte();
    f.colorB = buf.readByte();
    f.density = buf.readFloat();
    uint32_t n = buf.readUint32();
    f.children.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        RadialFamilyChild c;
        c.shaderTemplateName = assets::readNulTerminatedString(buf);
        c.weight = buf.readFloat();
        c.distance = buf.readFloat();
        c.width = buf.readFloat();
        c.height = buf.readFloat();
        c.shouldSway = buf.readUint32() != 0;
        c.displacement = buf.readFloat();
        c.period = buf.readFloat();
        c.alignToTerrain = buf.readUint32() != 0;
        c.createPlus = buf.readUint32() != 0;
        f.children.push_back(c);
    }
    return f;
}

std::vector<RadialFamily> parseRadialGroupForm(const assets::IffChunk& groupForm) {
    if (groupForm.children.empty() || groupForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: RadialGroup missing version FORM");
    }
    const assets::IffChunk& versionForm = groupForm.children[0];
    if (versionForm.formType != versionTag(3)) {
        throw std::runtime_error("terrain: RadialGroup unsupported version");
    }
    std::vector<RadialFamily> families;
    for (const auto& child : versionForm.children) {
        if (child.id != kTagRFAM) {
            throw std::runtime_error("terrain: RadialGroup: unexpected child");
        }
        soe::PacketBuffer buf = child.data;
        buf.resetReadCursor();
        families.push_back(parseRadialFamily(buf));
    }
    return families;
}

// -- EnvironmentGroup (FORM EGRP) --

EnvironmentFamily parseEnvironmentFamily(const assets::IffChunk& efamForm) {
    const assets::IffChunk* dataChunk = efamForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error("terrain: EnvironmentGroup family missing CHNK DATA");
    }
    soe::PacketBuffer buf = dataChunk->data;
    buf.resetReadCursor();
    EnvironmentFamily f;
    f.familyId = static_cast<int>(buf.readUint32());
    f.name = assets::readNulTerminatedString(buf);
    f.colorR = buf.readByte();
    f.colorG = buf.readByte();
    f.colorB = buf.readByte();
    f.featherClamp = buf.readFloat();
    return f;
}

std::vector<EnvironmentFamily> parseEnvironmentGroupForm(const assets::IffChunk& groupForm) {
    if (groupForm.children.empty() || groupForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: EnvironmentGroup missing version FORM");
    }
    const assets::IffChunk& versionForm = groupForm.children[0];
    // Versions 0000-0002 are byte-identical (confirmed by research) - any
    // of the three is accepted without narrowing to one.
    if (versionForm.formType != versionTag(0) && versionForm.formType != versionTag(1) &&
        versionForm.formType != versionTag(2)) {
        throw std::runtime_error("terrain: EnvironmentGroup unsupported version");
    }
    std::vector<EnvironmentFamily> families;
    for (const auto& child : versionForm.children) {
        if (child.id != assets::kFormTag || child.formType != kTagEFAM) {
            throw std::runtime_error("terrain: EnvironmentGroup: unexpected child");
        }
        families.push_back(parseEnvironmentFamily(child));
    }
    return families;
}

// -- FractalGroup / BitmapGroup (both FORM MGRP, distinguished by read order) --

FractalFamily parseFractalFamily(const assets::IffChunk& mfamForm) {
    const assets::IffChunk* dataChunk = mfamForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error("terrain: FractalGroup family missing CHNK DATA");
    }
    soe::PacketBuffer buf = dataChunk->data;
    buf.resetReadCursor();
    FractalFamily f;
    f.familyId = static_cast<int>(buf.readUint32());
    f.name = assets::readNulTerminatedString(buf);

    const assets::IffChunk* mfrcForm = findChildForm(mfamForm, kTagMFRC);
    if (mfrcForm == nullptr) {
        throw std::runtime_error("terrain: FractalGroup family missing FORM MFRC");
    }
    f.params = parseMultiFractalForm(*mfrcForm);
    return f;
}

BitmapFamily parseBitmapFamily(const assets::IffChunk& mfamForm) {
    const assets::IffChunk* dataChunk = mfamForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error("terrain: BitmapGroup family missing CHNK DATA");
    }
    soe::PacketBuffer buf = dataChunk->data;
    buf.resetReadCursor();
    BitmapFamily f;
    f.familyId = static_cast<int>(buf.readUint32());
    f.name = assets::readNulTerminatedString(buf);
    f.bitmapName = assets::readNulTerminatedString(buf);
    return f;
}

template <typename FamilyT, typename ParseFamilyFn>
std::vector<FamilyT> parseMgrpForm(const assets::IffChunk& groupForm, const char* typeName,
                                    ParseFamilyFn parseFamily) {
    if (groupForm.children.empty() || groupForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing version FORM");
    }
    const assets::IffChunk& versionForm = groupForm.children[0];
    if (versionForm.formType != versionTag(0)) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " unsupported version");
    }
    std::vector<FamilyT> families;
    for (const auto& child : versionForm.children) {
        if (child.id != assets::kFormTag || child.formType != kTagMFAM) {
            throw std::runtime_error(std::string("terrain: ") + typeName + ": unexpected child");
        }
        families.push_back(parseFamily(child));
    }
    return families;
}

} // namespace

FamilyGroups parseFamilyGroups(const assets::IffChunk& tgenVersionForm) {
    FamilyGroups result;
    int mgrpCount = 0;

    for (const auto& child : tgenVersionForm.children) {
        if (child.id != assets::kFormTag) {
            throw std::runtime_error("terrain: TGEN: unexpected non-FORM child");
        }
        try {
            switch (child.formType) {
                case kTagSGRP:
                    result.shaderFamilies = parseShaderGroupForm(child);
                    break;
                case kTagFGRP:
                    result.floraFamilies = parseFloraGroupForm(child);
                    break;
                case kTagRGRP:
                    result.radialFamilies = parseRadialGroupForm(child);
                    break;
                case kTagEGRP:
                    result.environmentFamilies = parseEnvironmentGroupForm(child);
                    break;
                case kTagMGRP:
                    if (mgrpCount == 0) {
                        result.fractalFamilies = parseMgrpForm<FractalFamily>(
                            child, "FractalGroup", parseFractalFamily);
                    } else if (mgrpCount == 1) {
                        result.bitmapFamilies =
                            parseMgrpForm<BitmapFamily>(child, "BitmapGroup", parseBitmapFamily);
                    } else {
                        throw std::runtime_error(
                            "terrain: TGEN: more than 2 MGRP forms encountered");
                    }
                    ++mgrpCount;
                    break;
                case kTagLYRS:
                    // Layer tree parsing is Layer.h's concern - skip here, the
                    // caller (TerrainGenerator::parse) handles LYRS separately.
                    break;
                default:
                    throw std::runtime_error("terrain: TGEN: unrecognized top-level child tag");
            }
        } catch (const std::exception& e) {
            char tagBuf[5] = {
                static_cast<char>((child.formType >> 24) & 0xFF),
                static_cast<char>((child.formType >> 16) & 0xFF),
                static_cast<char>((child.formType >> 8) & 0xFF),
                static_cast<char>(child.formType & 0xFF),
                0,
            };
            throw std::runtime_error(std::string("terrain: TGEN child ") + tagBuf + ": " +
                                      e.what());
        }
    }

    return result;
}

} // namespace terrain
