#include "terrain/LayerItem.h"

#include <stdexcept>

#include "GeneratorItem.h"
#include "Tags.h"

namespace terrain {

LayerItemHeader parseLayerItemHeader(const assets::IffChunk& ihdrForm) {
    if (ihdrForm.children.empty() || ihdrForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: IHDR missing version FORM");
    }
    const assets::IffChunk& versionForm = ihdrForm.children[0];
    const assets::IffChunk* dataChunk = versionForm.findChild(kTagDATA);
    if (dataChunk == nullptr) {
        throw std::runtime_error("terrain: IHDR missing CHNK DATA");
    }

    soe::PacketBuffer buf = dataChunk->data;
    buf.resetReadCursor();

    LayerItemHeader header;
    header.active = buf.readUint32() != 0;
    header.name = assets::readNulTerminatedString(buf);

    if (versionForm.formType == versionTag(0)) {
        // unused r,g,b tool color - read to keep the stream consistent,
        // value never meaningfully used by the real engine either.
        buf.readByte();
        buf.readByte();
        buf.readByte();
    } else if (versionForm.formType != versionTag(1)) {
        throw std::runtime_error("terrain: unrecognized IHDR version");
    }

    return header;
}

} // namespace terrain
