#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// A hardcoded constant in Core3's own source (CmdStartScene.h inserts this
// literal, not a computed String::hashCode) - do not try to derive it.
constexpr uint32_t kCmdStartSceneHash = 0x3AE6DFAE;

struct CmdStartScene {
    uint64_t selfObjectId = 0;
    std::string terrainName;
    float x = 0.0f;
    float y = 0.0f; // vertical height - see the wire-layout comment below
    float z = 0.0f; // the other horizontal (north-south) coordinate
    std::string templateFile; // the character's object template path, not a bare race name
    uint64_t galacticTime = 0;

    // Parses the fields following opCount+hash. Wire layout: byte (unknown,
    // always 0) + uint64 selfObjectId + ASCII terrainName + float x + float
    // (height) + float (other horizontal) + ASCII templateFile + uint64
    // galacticTime.
    //
    // Core3's own CmdStartScene.h sends `getX()`, `getZ()`, `getY()` (in
    // that order, its own inline comments literally say "//X" "//Z" "//Y")
    // from a raw `engine::util::u3d::Vector3`. Those accessor NAMES do not
    // match this project's (and the rest of this codebase's) x/y/z
    // convention used everywhere else (UpdateTransformMessage, CreatureObject
    // baselines/deltas, all rendered and visually confirmed correct many
    // times) - Vector3::getZ() is the one that's actually vertical height,
    // and Vector3::getY() is actually the second horizontal coordinate.
    // This was never visually validated before (CmdStartScene's position was
    // only ever printed to console, never fed into rendering) and was
    // discovered empirically: real captured (worldX, worldZ, expectedHeight)
    // samples from both a private dev server and Finalizer matched
    // terrain::TerrainGenerator::queryHeight(x, [2nd wire float]) almost
    // exactly (naboo: computed 14.423 vs. real 14.421; tatooine: computed
    // 5.000 vs. real 5.477) while the naive x/y/z-name interpretation was
    // off by three orders of magnitude. Fields here are named to match this
    // codebase's own convention (y=height, matching every other position
    // type), not Core3's raw Vector3 accessor names - the wire READ ORDER
    // is unchanged, only the field names/positions were swapped to match.
    static CmdStartScene parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
