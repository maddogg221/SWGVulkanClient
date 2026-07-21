#include "swgproto/ObjectStateDispatcher.h"

#include <iostream>

namespace swgproto {

namespace {

// Shared dispatch logic for both dispatchBaseline() and dispatchDelta() -
// they differ only in which registry/unknown-handler/error-message-label to
// use, not in the actual parse-lookup-call sequence. Returns true iff the
// envelope itself parsed successfully (see the header's comment on
// dispatchBaseline()/dispatchDelta() for why this matters to callers).
bool dispatchImpl(soe::PacketBuffer& buf,
                   const std::map<std::pair<std::string, uint8_t>, ObjectStateDispatcher::Handler>&
                       handlers,
                   const ObjectStateDispatcher::Handler& unknownHandler,
                   const char* messageTypeName) {
    auto envelope = BaselineEnvelope::parse(buf);
    if (!envelope.ok()) {
        std::cerr << "Dropping malformed " << messageTypeName << ": " << envelope.error() << "\n";
        return false;
    }

    const auto& env = envelope.value();
    auto key = std::make_pair(objectTypeTag(env.objectType), env.baselineNumber);

    try {
        auto it = handlers.find(key);
        if (it != handlers.end()) {
            it->second(env, buf);
        } else if (unknownHandler) {
            unknownHandler(env, buf);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to handle " << messageTypeName << " (" << key.first << " baseline "
                   << static_cast<int>(key.second) << "): " << e.what() << "\n";
    }
    return true;
}

} // namespace

void ObjectStateDispatcher::onBaseline(const std::string& objectType, uint8_t baselineNumber,
                                        Handler handler) {
    baselineHandlers_[{objectType, baselineNumber}] = std::move(handler);
}

void ObjectStateDispatcher::offBaseline(const std::string& objectType, uint8_t baselineNumber) {
    baselineHandlers_.erase({objectType, baselineNumber});
}

void ObjectStateDispatcher::onUnknownBaseline(Handler handler) {
    unknownBaselineHandler_ = std::move(handler);
}

void ObjectStateDispatcher::offUnknownBaseline() {
    unknownBaselineHandler_ = nullptr;
}

void ObjectStateDispatcher::onDelta(const std::string& objectType, uint8_t baselineNumber,
                                     Handler handler) {
    deltaHandlers_[{objectType, baselineNumber}] = std::move(handler);
}

void ObjectStateDispatcher::offDelta(const std::string& objectType, uint8_t baselineNumber) {
    deltaHandlers_.erase({objectType, baselineNumber});
}

void ObjectStateDispatcher::onUnknownDelta(Handler handler) {
    unknownDeltaHandler_ = std::move(handler);
}

void ObjectStateDispatcher::offUnknownDelta() {
    unknownDeltaHandler_ = nullptr;
}

bool ObjectStateDispatcher::dispatchBaseline(soe::PacketBuffer& buf) {
    return dispatchImpl(buf, baselineHandlers_, unknownBaselineHandler_, "BaselinesMessage");
}

bool ObjectStateDispatcher::dispatchDelta(soe::PacketBuffer& buf) {
    return dispatchImpl(buf, deltaHandlers_, unknownDeltaHandler_, "DeltasMessage");
}

} // namespace swgproto
