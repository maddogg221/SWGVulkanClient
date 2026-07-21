#include "assets/BuildingLayout.h"

#include <optional>
#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kPrtoTag = 0x5052544F; // 'PRTO'
constexpr uint32_t kCelsTag = 0x43454C53; // 'CELS'
constexpr uint32_t kDataTag = 0x44415441; // 'DATA'
constexpr uint32_t kCmshTag = 0x434D5348; // 'CMSH' (Collision Mesh)
constexpr uint32_t kIdtlTag = 0x4944544C; // 'IDTL'
constexpr uint32_t kVertTag = 0x56455254; // 'VERT'
constexpr uint32_t kIndxTag = 0x494E4458; // 'INDX'
constexpr uint32_t kFlorTag = 0x464C4F52; // 'FLOR' (real .flr root FORM)
constexpr uint32_t kTrisTag = 0x54524953; // 'TRIS' (real .flr triangle chunk)
// Real, confirmed via a direct byte dump this session: 'PRTL' is used as
// BOTH a leaf CHNK id (each real portal SHAPE, directly under the
// building-wide FORM(PRTS)) AND, confusingly, as a FORM's own formType
// (each cell's own per-portal PLACEMENT wrapper) - two different chunk
// KINDS sharing the same 4-char tag in different contexts. kPrtsTag/
// kPrtlTag below are used for both meanings; callers disambiguate by
// whether they're looking at a FORM or a leaf CHNK.
constexpr uint32_t kPrtsTag = 0x50525453;             // 'PRTS'
constexpr uint32_t kPrtlTag = 0x5052544C;             // 'PRTL'
constexpr uint32_t kPortalPlacementTag = 0x30303034;  // '0004'

// Real per-cell portal PLACEMENT record (Phase 20b) - a fixed 60-byte
// CHNK("0004") nested one level inside each of a cell's own FORM(PRTL)
// wrappers. Real, confirmed byte layout (direct dump, Eese's House, every
// portal checked): byte[0] a constant 1 (purpose not chased down), byte[1]
// the real portal SHAPE index (into BuildingLayoutData::portalShapes,
// shared building-wide), bytes[2..4] always zero, byte[5] a 0/1 flag that
// flips between the two cells sharing the same portal (purpose not chased
// down - plausibly orientation/winding), byte[6] the real ADJACENT CELL
// index, bytes[7..11] always zero, followed by 12 real floats (a 3x3
// rotation matrix + a Float3 translation) that were exactly identity/zero
// in every real portal checked - consistent with this project's own
// established fact that every cell already shares one common building-
// local coordinate frame (see worldToBuildingLocal()'s own comment in
// Visualizer.cpp), so this transform is read but not applied. Never
// throws - a malformed/too-short record is simply skipped (this cell just
// has one fewer known portal), matching this parser's own established
// per-part skip convention.
std::optional<CellPortal> readPortalPlacement(const IffChunk& prtlForm) {
    const IffChunk* dataChunk = findFirstChunk(prtlForm, kPortalPlacementTag);
    if (dataChunk == nullptr || dataChunk->data.size() < 12) {
        return std::nullopt;
    }
    soe::PacketBuffer buf = dataChunk->data;
    buf.resetReadCursor();
    buf.readByte(); // byte[0], constant 1, meaning not chased down
    CellPortal portal;
    portal.portalShapeIndex = buf.readByte(); // byte[1]
    buf.readByte();
    buf.readByte();
    buf.readByte(); // bytes[2..4], always zero
    buf.readByte(); // byte[5], orientation/direction flag, not chased down
    portal.adjacentCellIndex = buf.readByte(); // byte[6]
    // bytes[7..11] and the trailing 48-byte identity rotation + zero
    // translation - unread, see this function's own comment.
    return portal;
}

// Every real FORM(PRTL) placement wrapper directly under a cell's own
// FORM(CELL) block (recursive descent via findAllForms, same pattern
// readCollisionMesh() already uses for CMSH) - count matches the cell's
// own numCellPortals field. A malformed individual placement is simply
// skipped (this cell just has one fewer known portal), never fails the
// whole cell.
std::vector<CellPortal> readCellPortals(const IffChunk& cellForm) {
    std::vector<CellPortal> result;
    for (const IffChunk* prtlForm : findAllForms(cellForm, kPrtlTag)) {
        auto portal = readPortalPlacement(*prtlForm);
        if (portal.has_value()) {
            result.push_back(*portal);
        }
    }
    return result;
}

// The real, shared, building-wide list of portal opening SHAPES (FORM
// PRTS's own children, each a leaf CHNK(PRTL) - a flat, unprefixed-count
// Float3 list of vertices, mirroring CMSH's own VERT convention). Index
// alignment with FORM(PRTS)'s own child order is preserved even for a
// malformed shape (pushed as an empty PortalShape) since
// BuildingCell::portals[].portalShapeIndex refers to this list
// positionally.
std::vector<PortalShape> readPortalShapes(const IffChunk& root) {
    std::vector<PortalShape> result;
    const IffChunk* prtsForm = findFirstForm(root, kPrtsTag);
    if (prtsForm == nullptr) {
        return result;
    }
    for (const IffChunk& child : prtsForm->children) {
        PortalShape shape;
        if (child.id == kPrtlTag && child.data.size() >= 4) {
            soe::PacketBuffer buf = child.data;
            buf.resetReadCursor();
            uint32_t count = buf.readUint32();
            bool ok = true;
            for (uint32_t i = 0; i < count && ok; ++i) {
                if (buf.remaining() < 12) {
                    ok = false;
                    break;
                }
                Float3 p;
                p.x = buf.readFloat();
                p.y = buf.readFloat();
                p.z = buf.readFloat();
                shape.vertices.push_back(p);
            }
            if (!ok) {
                shape.vertices.clear(); // malformed - keep index alignment, empty shape
            }
        }
        result.push_back(std::move(shape));
    }
    return result;
}

// Reads a real cell's inline collision mesh, if present. Real, confirmed
// format (see BuildingCell::collisionMesh's own comment): VERT is a flat
// array of Float3 positions with no leading count or header, INDX is a flat
// array of uint32 triangle indices, also with no leading count - simpler
// than StaticMesh's own VTXA/INDX shape. Never throws - any missing chunk
// or a byte size that doesn't divide evenly (12 bytes/vertex, 4
// bytes/index) just returns an empty MeshData, matching this project's
// established "skip, don't fail the whole object" convention for optional
// real data.
MeshData readCollisionMesh(const IffChunk& cellForm) {
    MeshData result;
    const IffChunk* cmshForm = findFirstForm(cellForm, kCmshTag);
    if (cmshForm == nullptr) {
        return result;
    }
    const IffChunk* idtlForm = findFirstForm(*cmshForm, kIdtlTag);
    if (idtlForm == nullptr) {
        return result;
    }
    const IffChunk* vertChunk = findFirstChunk(*idtlForm, kVertTag);
    const IffChunk* indxChunk = findFirstChunk(*idtlForm, kIndxTag);
    if (vertChunk == nullptr || indxChunk == nullptr) {
        return result;
    }
    if (vertChunk->data.size() % 12 != 0 || indxChunk->data.size() % 4 != 0) {
        return result;
    }

    size_t numVerts = vertChunk->data.size() / 12;
    soe::PacketBuffer vBuf = vertChunk->data;
    vBuf.resetReadCursor();
    result.positions.reserve(numVerts);
    for (size_t i = 0; i < numVerts; ++i) {
        Float3 p;
        p.x = vBuf.readFloat();
        p.y = vBuf.readFloat();
        p.z = vBuf.readFloat();
        result.positions.push_back(p);
    }

    size_t numIndices = indxChunk->data.size() / 4;
    soe::PacketBuffer iBuf = indxChunk->data;
    iBuf.resetReadCursor();
    result.indices.reserve(numIndices);
    for (size_t i = 0; i < numIndices; ++i) {
        result.indices.push_back(iBuf.readUint32());
    }

    // A real index referencing past the end of the real positions array
    // would make this mesh unsafe to raycast against - treat that as "no
    // usable collision data" rather than trusting partially-malformed
    // input, same defensive posture as everywhere else in this parser.
    for (uint32_t index : result.indices) {
        if (index >= result.positions.size()) {
            return MeshData{};
        }
    }

    return result;
}
} // namespace

BuildingLayoutData BuildingLayout::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kPrtoTag) {
        throw std::runtime_error("BuildingLayout::parse: not a FORM(PRTO)-rooted file");
    }
    const IffChunk& root = topLevel[0];

    // Top-level DATA (numPortals/numCells) is a direct child of the version
    // FORM, encountered before any per-cell DATA chunk in document order -
    // findFirstChunk's pre-order search is safe here for exactly that
    // reason (see BuildingLayout.h's own comment).
    const IffChunk* topData = findFirstChunk(root, kDataTag);
    if (topData == nullptr) {
        throw std::runtime_error("BuildingLayout::parse: top-level DATA chunk not found");
    }
    soe::PacketBuffer topDataBuf = topData->data;
    topDataBuf.resetReadCursor();
    topDataBuf.readUint32(); // numPortals - not read (matches portalShapes.size() when present)
    uint32_t numCells = topDataBuf.readUint32();

    const IffChunk* celsForm = findFirstForm(root, kCelsTag);
    if (celsForm == nullptr) {
        throw std::runtime_error("BuildingLayout::parse: FORM CELS not found");
    }

    BuildingLayoutData result;
    result.cells.reserve(numCells);
    result.portalShapes = readPortalShapes(root);
    for (const IffChunk& cellForm : celsForm->children) {
        const IffChunk* cellData = findFirstChunk(cellForm, kDataTag);
        if (cellData == nullptr) {
            throw std::runtime_error("BuildingLayout::parse: cell DATA chunk not found");
        }
        soe::PacketBuffer buf = cellData->data;
        buf.resetReadCursor();
        buf.readUint32(); // numCellPortals - not read (matches cell.portals.size() when present)
        buf.readByte();   // unread - meaning not chased down, not needed here
        BuildingCell cell;
        cell.name = readNulTerminatedString(buf);
        cell.modelFilename = readNulTerminatedString(buf);

        // hasFloor + floorCollisionFilename (Phase 20) - real, confirmed
        // present (hasFloor=1) on all 18 real cells checked so far, but
        // treated as genuinely optional here (a cell predating this real
        // field, or a malformed one, simply gets an empty
        // floorCollisionFilename) rather than assumed universal.
        if (buf.remaining() > 0) {
            uint8_t hasFloor = buf.readByte();
            if (hasFloor != 0 && buf.remaining() > 0) {
                cell.floorCollisionFilename = readNulTerminatedString(buf);
            }
        }

        cell.collisionMesh = readCollisionMesh(cellForm);
        cell.portals = readCellPortals(cellForm);

        result.cells.push_back(std::move(cell));
    }

    if (result.cells.size() != numCells) {
        throw std::runtime_error("BuildingLayout::parse: cell count mismatch");
    }

    return result;
}

// Real, confirmed byte format (direct dump this session, a real stairwell's
// own .flr file): CHNK(VERT) is a leading uint32 count followed by that
// many real Float3 positions (unlike CMSH's own VERT, which has no leading
// count) - same per-vertex layout as everywhere else in this project
// otherwise. CHNK(TRIS) is a leading uint32 count followed by that many
// rich 60-byte real records; only the first 12 bytes of each (3 real
// uint32 vertex indices) are read here - the remaining 48 bytes (real
// triangle-adjacency indices, a plane-equation-shaped float pair, and more
// indices, empirically confirmed present but not needed for a plain 2D
// point-in-triangle query) are skipped, matching this project's
// established "read only what's needed" convention.
FloorCollisionMesh FloorCollision::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kFlorTag) {
        throw std::runtime_error("FloorCollision::parse: not a FORM(FLOR)-rooted file");
    }
    const IffChunk& root = topLevel[0];

    const IffChunk* vertChunk = findFirstChunk(root, kVertTag);
    const IffChunk* trisChunk = findFirstChunk(root, kTrisTag);
    if (vertChunk == nullptr || trisChunk == nullptr) {
        throw std::runtime_error("FloorCollision::parse: VERT/TRIS chunk not found");
    }

    FloorCollisionMesh result;

    if (vertChunk->data.size() >= 4) {
        soe::PacketBuffer vBuf = vertChunk->data;
        vBuf.resetReadCursor();
        uint32_t vertCount = vBuf.readUint32();
        if (vBuf.remaining() >= static_cast<size_t>(vertCount) * 12) {
            result.positions.reserve(vertCount);
            for (uint32_t i = 0; i < vertCount; ++i) {
                Float3 p;
                p.x = vBuf.readFloat();
                p.y = vBuf.readFloat();
                p.z = vBuf.readFloat();
                result.positions.push_back(p);
            }
        }
    }

    constexpr size_t kTriangleRecordBytes = 60;
    if (trisChunk->data.size() >= 4) {
        soe::PacketBuffer tBuf = trisChunk->data;
        tBuf.resetReadCursor();
        uint32_t triCount = tBuf.readUint32();
        if (tBuf.remaining() >= static_cast<size_t>(triCount) * kTriangleRecordBytes) {
            result.triangleVertexIndices.reserve(static_cast<size_t>(triCount) * 3);
            result.triangleNeighbors.reserve(static_cast<size_t>(triCount) * 3);
            for (uint32_t i = 0; i < triCount; ++i) {
                uint32_t v0 = tBuf.readUint32();
                uint32_t v1 = tBuf.readUint32();
                uint32_t v2 = tBuf.readUint32();
                result.triangleVertexIndices.push_back(v0);
                result.triangleVertexIndices.push_back(v1);
                result.triangleVertexIndices.push_back(v2);
                tBuf.readUint32(); // real field, purpose not chased down - unread
                // Real per-edge neighbor triangle indices (bytes 16..28 of
                // this 60-byte record) - confirmed via a direct byte dump
                // this session: two triangles sharing a real edge
                // cross-reference each other's index here, -1 (0xFFFFFFFF)
                // for a real mesh boundary edge.
                result.triangleNeighbors.push_back(static_cast<int32_t>(tBuf.readUint32()));
                result.triangleNeighbors.push_back(static_cast<int32_t>(tBuf.readUint32()));
                result.triangleNeighbors.push_back(static_cast<int32_t>(tBuf.readUint32()));
                tBuf.readBytes(kTriangleRecordBytes - 12 - 4 - 12); // skip the rest of this record
            }
        }
    }

    // A real index referencing past the end of the real positions array
    // would make this mesh unsafe to query against - same defensive
    // posture as readCollisionMesh()'s own equivalent check.
    for (uint32_t index : result.triangleVertexIndices) {
        if (index >= result.positions.size()) {
            return FloorCollisionMesh{};
        }
    }

    return result;
}

} // namespace assets
