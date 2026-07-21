#include "swgproto/GroupObjectDelta6.h"

#include <exception>

namespace swgproto {

namespace {

bool applyMemberOp(std::vector<GroupMemberEntry>& members, soe::PacketBuffer& buf,
                    std::string& error) {
    uint8_t tag = buf.readByte();
    switch (tag) {
        case 1: // ADD ("Initial")
        case 2: { // UPDATE
            uint16_t index = buf.readUint16();
            GroupMemberEntry entry;
            entry.objectId = buf.readUint64();
            entry.name = buf.readAscii();
            if (index >= members.size()) {
                members.resize(index + 1);
            }
            members[index] = std::move(entry);
            return true;
        }
        case 0: { // REMOVE
            uint16_t index = buf.readUint16();
            if (index < members.size()) {
                members.erase(members.begin() + index);
            }
            return true;
        }
        case 4: // CLEAR
            members.clear();
            return true;
        default:
            error = "unknown member op tag " + std::to_string(static_cast<int>(tag));
            return false;
    }
}

bool applyShipOp(std::vector<GroupShipEntry>& ships, soe::PacketBuffer& buf, std::string& error) {
    uint8_t tag = buf.readByte();
    switch (tag) {
        case 1:
        case 2: {
            uint16_t index = buf.readUint16();
            GroupShipEntry entry;
            entry.shipId = buf.readUint64();
            entry.index = static_cast<int32_t>(buf.readUint32());
            if (index >= ships.size()) {
                ships.resize(index + 1);
            }
            ships[index] = entry;
            return true;
        }
        case 0: {
            uint16_t index = buf.readUint16();
            if (index < ships.size()) {
                ships.erase(ships.begin() + index);
            }
            return true;
        }
        case 4:
            ships.clear();
            return true;
        default:
            error = "unknown ship op tag " + std::to_string(static_cast<int>(tag));
            return false;
    }
}

// Reads the shared opCount+updateCounter header, then `opCount` entries via
// `applyOp`. Returns false with `error` set on any failure.
template <typename ApplyOpFn>
bool applyListDelta(soe::PacketBuffer& buf, std::string& error, ApplyOpFn&& applyOp) {
    uint32_t opCount = buf.readUint32();
    buf.readUint32(); // updateCounter - discarded
    for (uint32_t i = 0; i < opCount; ++i) {
        if (!applyOp()) {
            return false;
        }
    }
    (void)error;
    return true;
}

} // namespace

GroupObjectDeltaResult applyGroupObjectBaseline6Delta(GroupObjectBaseline6& state, uint16_t count,
                                                        soe::PacketBuffer& buf) {
    GroupObjectDeltaResult result;
    result.totalCount = count;

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t fieldIndex = 0;
        std::string error;
        bool applied = false;
        try {
            fieldIndex = buf.readUint16();
            switch (fieldIndex) {
                case 0x01:
                    applied = applyListDelta(buf, error, [&]() {
                        return applyMemberOp(state.members, buf, error);
                    });
                    break;
                case 0x02:
                    applied = applyListDelta(
                        buf, error, [&]() { return applyShipOp(state.ships, buf, error); });
                    break;
                case 0x03:
                    state.groupName = buf.readAscii();
                    applied = true;
                    break;
                case 0x04:
                    state.groupLevel = buf.readUint16();
                    applied = true;
                    break;
                case 0x06:
                    state.masterLooterId = buf.readUint64();
                    applied = true;
                    break;
                case 0x07:
                    state.lootRule = static_cast<int32_t>(buf.readUint32());
                    applied = true;
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
