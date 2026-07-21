#include "terrain/Filter.h"

#include <cmath>
#include <numbers>
#include <stdexcept>

#include "GeneratorItem.h"
#include "Tags.h"

namespace terrain {

namespace {

float degreesToRadians(float degrees) {
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

Filter parseFilterHeight(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(2), "FilterHeight");
    FilterHeight f;
    f.lowHeight = v.data.readFloat();
    f.highHeight = v.data.readFloat();
    f.featherFunction = static_cast<int>(v.data.readUint32());
    f.featherDistance = readClampedFeatherDistance(v.data);
    return Filter{v.header, f};
}

Filter parseFilterFractal(const assets::IffChunk& form) {
    VersionedPayload v = enterVersionedNestedParm(form, versionTag(5), "FilterFractal");
    FilterFractal f;
    f.familyId = static_cast<int>(v.data.readUint32());
    f.featherFunction = static_cast<int>(v.data.readUint32());
    f.featherDistance = readClampedFeatherDistance(v.data);
    f.lowFractalLimit = v.data.readFloat();
    f.highFractalLimit = v.data.readFloat();
    f.scaleY = v.data.readFloat();
    return Filter{v.header, f};
}

// Not real-bytes verified - never observed in any of the 10 classic
// planets' .trn files. Ported from source's current/latest version.
Filter parseFilterBitmap(const assets::IffChunk& form) {
    VersionedPayload v = enterVersionedNestedParm(form, versionTag(1), "FilterBitmap");
    FilterBitmap f;
    f.familyId = static_cast<int>(v.data.readUint32());
    f.featherFunction = static_cast<int>(v.data.readUint32());
    f.featherDistance = readClampedFeatherDistance(v.data);
    f.lowBitmapLimit = v.data.readFloat();
    f.highBitmapLimit = v.data.readFloat();
    f.gain = v.data.readFloat();
    return Filter{v.header, f};
}

Filter parseFilterSlope(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(2), "FilterSlope");
    FilterSlope f;
    f.minimumAngleRadians = degreesToRadians(v.data.readFloat());
    f.maximumAngleRadians = degreesToRadians(v.data.readFloat());
    f.featherFunction = static_cast<int>(v.data.readUint32());
    f.featherDistance = readClampedFeatherDistance(v.data);
    return Filter{v.header, f};
}

Filter parseFilterDirection(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "FilterDirection");
    FilterDirection f;
    f.minimumAngleRadians = degreesToRadians(v.data.readFloat());
    f.maximumAngleRadians = degreesToRadians(v.data.readFloat());
    f.featherFunction = static_cast<int>(v.data.readUint32());
    f.featherDistance = readClampedFeatherDistance(v.data);
    return Filter{v.header, f};
}

Filter parseFilterShader(const assets::IffChunk& form) {
    VersionedPayload v = enterVersioned(form, versionTag(0), "FilterShader");
    FilterShader f;
    f.familyId = static_cast<int>(v.data.readUint32());
    return Filter{v.header, f};
}

} // namespace

bool isFilterTag(uint32_t tag) {
    return tag == kTagFHGT || tag == kTagFFRA || tag == kTagFBIT || tag == kTagFSLP ||
           tag == kTagFDIR || tag == kTagFSHD;
}

Filter parseFilter(const assets::IffChunk& form) {
    switch (form.formType) {
        case kTagFHGT:
            return parseFilterHeight(form);
        case kTagFFRA:
            return parseFilterFractal(form);
        case kTagFBIT:
            return parseFilterBitmap(form);
        case kTagFSLP:
            return parseFilterSlope(form);
        case kTagFDIR:
            return parseFilterDirection(form);
        case kTagFSHD:
            return parseFilterShader(form);
        default:
            throw std::runtime_error("terrain: parseFilter called with a non-filter tag");
    }
}

} // namespace terrain
