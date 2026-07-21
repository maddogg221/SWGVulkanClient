#include "swgproto/SchemaEngine.h"

#include <exception>
#include <sstream>

namespace swgproto {

bool decodeBaselineInto(const ObjectSchema& schema, void* base, soe::PacketBuffer& buf,
                         std::string& error) {
    if (schema.ancestor != nullptr) {
        void* ancestorBase = static_cast<char*>(base) + schema.ancestorOffset;
        if (!decodeBaselineInto(*schema.ancestor, ancestorBase, buf, error)) {
            return false;
        }
    }

    for (const auto& f : schema.ownFields) {
        if (!f.decodeInto(base, buf, error)) {
            error = std::string(f.name) + ": " + error;
            return false;
        }
    }

    return true;
}

DeltaFieldLookup findDeltaField(const ObjectSchema& schema, uint16_t fieldIndex) {
    for (const auto& f : schema.ownFields) {
        if (f.deltaAddressable && f.index == fieldIndex) {
            return DeltaFieldLookup{&f};
        }
    }
    if (schema.ancestor != nullptr) {
        return findDeltaField(*schema.ancestor, fieldIndex);
    }
    return DeltaFieldLookup{nullptr};
}

DeltaDecodeResult decodeDeltaMessage(const ObjectSchema& schema, uint16_t count,
                                      soe::PacketBuffer& buf) {
    DeltaDecodeResult result;
    result.totalCount = count;

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t fieldIndex = 0;
        try {
            fieldIndex = buf.readUint16();
        } catch (const std::exception& e) {
            result.stoppedEarly = true;
            result.stopReason = "buffer too short reading update " + std::to_string(i) +
                                 "'s field index: " + e.what();
            return result;
        }

        auto lookup = findDeltaField(schema, fieldIndex);
        if (lookup.descriptor == nullptr) {
            std::ostringstream oss;
            oss << "unknown field index 0x" << std::hex << fieldIndex << " at update " << std::dec
                << i;
            result.stoppedEarly = true;
            result.stopReason = oss.str();
            return result;
        }

        DeltaFieldUpdate update;
        update.fieldIndex = fieldIndex;
        update.fieldName = lookup.descriptor->name;

        std::string error;
        if (!lookup.descriptor->decodeAndFormat(buf, update.valueText, error)) {
            result.stoppedEarly = true;
            result.stopReason = "field " + update.fieldName + ": " + error;
            return result;
        }

        result.updates.push_back(std::move(update));
    }

    return result;
}

DeltaFieldLookupWithOffset findDeltaFieldWithOffset(const ObjectSchema& schema,
                                                     uint16_t fieldIndex, size_t offsetSoFar) {
    for (const auto& f : schema.ownFields) {
        if (f.deltaAddressable && f.index == fieldIndex) {
            return DeltaFieldLookupWithOffset{&f, offsetSoFar};
        }
    }
    if (schema.ancestor != nullptr) {
        return findDeltaFieldWithOffset(*schema.ancestor, fieldIndex,
                                         offsetSoFar + schema.ancestorOffset);
    }
    return DeltaFieldLookupWithOffset{nullptr, 0};
}

DeltaApplyResult applyDeltaMessage(const ObjectSchema& schema, void* base, uint16_t count,
                                    soe::PacketBuffer& buf) {
    DeltaApplyResult result;
    result.totalCount = count;

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t fieldIndex = 0;
        try {
            fieldIndex = buf.readUint16();
        } catch (const std::exception& e) {
            result.stoppedEarly = true;
            result.stopReason = "buffer too short reading update " + std::to_string(i) +
                                 "'s field index: " + e.what();
            return result;
        }

        auto lookup = findDeltaFieldWithOffset(schema, fieldIndex);
        if (lookup.descriptor == nullptr) {
            std::ostringstream oss;
            oss << "unknown field index 0x" << std::hex << fieldIndex << " at update " << std::dec
                << i;
            result.stoppedEarly = true;
            result.stopReason = oss.str();
            return result;
        }

        std::string error;
        void* fieldBase = static_cast<char*>(base) + lookup.offset;
        if (!lookup.descriptor->decodeInto(fieldBase, buf, error)) {
            result.stoppedEarly = true;
            result.stopReason = "field " + std::string(lookup.descriptor->name) + ": " + error;
            return result;
        }

        result.appliedFieldIndices.push_back(fieldIndex);
    }

    return result;
}

} // namespace swgproto
