#include "swgproto/HarvesterObjectDelta7.h"

#include <exception>

namespace swgproto {

namespace {

bool applyHopperOp(std::vector<HarvesterHopperEntry>& hopperList, soe::PacketBuffer& buf,
                    std::string& error) {
    uint8_t tag = buf.readByte();
    switch (tag) {
        case 1: // ADD
        case 2: { // SET
            uint16_t index = buf.readUint16();
            HarvesterHopperEntry entry;
            entry.resourceSpawnId = buf.readUint64();
            entry.quantity = buf.readFloat();
            if (index >= hopperList.size()) {
                hopperList.resize(index + 1);
            }
            hopperList[index] = entry;
            return true;
        }
        case 0: { // REMOVE
            uint16_t index = buf.readUint16();
            if (index < hopperList.size()) {
                hopperList.erase(hopperList.begin() + index);
            }
            return true;
        }
        case 4: // CLEAR
            hopperList.clear();
            return true;
        default:
            error = "unknown hopper op tag " + std::to_string(static_cast<int>(tag));
            return false;
    }
}

// Reads the shared opCount+updateCounter header, then `opCount` entries.
bool applyHopperListDelta(soe::PacketBuffer& buf, std::vector<HarvesterHopperEntry>& hopperList,
                           std::string& error) {
    uint32_t opCount = buf.readUint32();
    buf.readUint32(); // updateCounter - discarded
    for (uint32_t i = 0; i < opCount; ++i) {
        if (!applyHopperOp(hopperList, buf, error)) {
            return false;
        }
    }
    return true;
}

} // namespace

HarvesterObjectDeltaResult applyHarvesterObjectBaseline7Delta(HarvesterObjectBaseline7& state,
                                                                uint16_t count, soe::PacketBuffer& buf) {
    HarvesterObjectDeltaResult result;
    result.totalCount = count;

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t fieldIndex = 0;
        std::string error;
        bool applied = false;
        try {
            fieldIndex = buf.readUint16();
            switch (fieldIndex) {
                case 0x05:
                    state.activeResourceSpawnId = buf.readUint64();
                    applied = true;
                    break;
                case 0x06:
                    state.isActive = buf.readByte() != 0;
                    applied = true;
                    break;
                case 0x09:
                    state.actualExtractRate = buf.readFloat();
                    applied = true;
                    break;
                case 0x0A:
                    state.hopperSize = buf.readFloat();
                    applied = true;
                    break;
                case 0x0C:
                    buf.readByte(); // scalar "hopper changed" flag - not separately modeled
                    applied = true;
                    break;
                case 0x0D:
                    applied = applyHopperListDelta(buf, state.hopperList, error);
                    break;
                default:
                    error = "unmapped field index " + std::to_string(fieldIndex);
                    break;
            }
        } catch (const std::exception& e) {
            error = e.what();
            applied = false;
        }

        if (!applied) {
            result.stoppedEarly = true;
            result.stopReason = error;
            break;
        }
        result.appliedFieldIndices.push_back(fieldIndex);
    }

    return result;
}

} // namespace swgproto
