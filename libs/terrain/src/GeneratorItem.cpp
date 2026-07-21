#include "GeneratorItem.h"

#include <algorithm>

namespace terrain {

namespace {

const assets::IffChunk& firstIhdrForm(const assets::IffChunk& versionForm, const char* typeName) {
    if (versionForm.children.empty() || versionForm.children[0].id != assets::kFormTag ||
        versionForm.children[0].formType != kTagIHDR) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing IHDR");
    }
    return versionForm.children[0];
}

const assets::IffChunk& versionFormOf(const assets::IffChunk& form, uint32_t expectedVersion,
                                       const char* typeName) {
    if (form.children.empty() || form.children[0].id != assets::kFormTag) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing version FORM");
    }
    const assets::IffChunk& versionForm = form.children[0];
    if (versionForm.formType != expectedVersion) {
        throw std::runtime_error(std::string("terrain: ") + typeName +
                                  " unsupported version (real files only use one)");
    }
    return versionForm;
}

} // namespace

const assets::IffChunk* findChildForm(const assets::IffChunk& parent, uint32_t formTypeTag) {
    for (const auto& child : parent.children) {
        if (child.id == assets::kFormTag && child.formType == formTypeTag) {
            return &child;
        }
    }
    return nullptr;
}

VersionedPayload enterVersioned(const assets::IffChunk& form, uint32_t expectedVersion,
                                 const char* typeName) {
    const assets::IffChunk& versionForm = versionFormOf(form, expectedVersion, typeName);

    VersionedPayload result;
    result.header = parseLayerItemHeader(firstIhdrForm(versionForm, typeName));

    const assets::IffChunk* dataChunk = versionForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing CHNK DATA");
    }
    result.data = dataChunk->data;
    result.data.resetReadCursor();
    return result;
}

VersionedPayload enterVersionedNestedParm(const assets::IffChunk& form, uint32_t expectedVersion,
                                           const char* typeName) {
    const assets::IffChunk& versionForm = versionFormOf(form, expectedVersion, typeName);

    VersionedPayload result;
    result.header = parseLayerItemHeader(firstIhdrForm(versionForm, typeName));

    const assets::IffChunk* dataForm = findChildForm(versionForm, kTagDATA);
    if (dataForm == nullptr) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing FORM DATA");
    }
    const assets::IffChunk* parmChunk = dataForm->findChild(kTagPARM);
    if (parmChunk == nullptr) {
        throw std::runtime_error(std::string("terrain: ") + typeName + " missing CHNK PARM");
    }
    result.data = parmChunk->data;
    result.data.resetReadCursor();
    return result;
}

float readClampedFeatherDistance(soe::PacketBuffer& buf) {
    return std::clamp(buf.readFloat(), 0.0f, 1.0f);
}

std::vector<assets::Float2> readPointList2D(soe::PacketBuffer& buf) {
    uint32_t n = buf.readUint32();
    std::vector<assets::Float2> points;
    points.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        assets::Float2 p;
        p.x = buf.readFloat();
        p.y = buf.readFloat();
        points.push_back(p);
    }
    return points;
}

} // namespace terrain
