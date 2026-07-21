#include "assets/BuildingLayout.h"

#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kPrtoTag = 0x5052544F; // 'PRTO'
constexpr uint32_t kCelsTag = 0x43454C53; // 'CELS'
constexpr uint32_t kDataTag = 0x44415441; // 'DATA'
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
    topDataBuf.readUint32(); // numPortals - not read (portal geometry out of scope this pass)
    uint32_t numCells = topDataBuf.readUint32();

    const IffChunk* celsForm = findFirstForm(root, kCelsTag);
    if (celsForm == nullptr) {
        throw std::runtime_error("BuildingLayout::parse: FORM CELS not found");
    }

    BuildingLayoutData result;
    result.cells.reserve(numCells);
    for (const IffChunk& cellForm : celsForm->children) {
        const IffChunk* cellData = findFirstChunk(cellForm, kDataTag);
        if (cellData == nullptr) {
            throw std::runtime_error("BuildingLayout::parse: cell DATA chunk not found");
        }
        soe::PacketBuffer buf = cellData->data;
        buf.resetReadCursor();
        buf.readUint32(); // numCellPortals - not read (portal placement out of scope this pass)
        buf.readByte();   // unread - meaning not chased down, not needed here
        BuildingCell cell;
        cell.name = readNulTerminatedString(buf);
        cell.modelFilename = readNulTerminatedString(buf);
        result.cells.push_back(std::move(cell));
    }

    if (result.cells.size() != numCells) {
        throw std::runtime_error("BuildingLayout::parse: cell count mismatch");
    }

    return result;
}

} // namespace assets
