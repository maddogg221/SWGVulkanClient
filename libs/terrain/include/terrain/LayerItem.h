#pragma once

#include <string>

#include "assets/IffReader.h"

namespace terrain {

// The common `FORM IHDR` header every Boundary/Filter/Affector/Layer begins
// with (confirmed identical across all of them in the real engine source -
// every concrete load() calls the same LayerItem::load() first). Real
// classic-planet files exclusively use IHDR version 0001 (confirmed: all
// 2054 real layers across all 10 classic .trn files use IHDR/0001, zero
// exceptions - see this library's real-bytes version scan, noted in the
// terrain plan's Phase 3 commit). Version 0000 (adds an unused r,g,b tool
// color after name, never populated meaningfully by the real editor) is
// still supported since it costs nothing extra and the wire format is fully
// known from source - unlike elsewhere in this library, there was no reason
// to narrow this one to a single version.
struct LayerItemHeader {
    bool active = true;
    std::string name;
};

// `ihdrForm` is the `FORM IHDR` node itself (i.e. what a child dispatch
// loop finds when `child.formType == kTagIHDR`). Throws std::runtime_error
// for an unrecognized IHDR version.
LayerItemHeader parseLayerItemHeader(const assets::IffChunk& ihdrForm);

} // namespace terrain
