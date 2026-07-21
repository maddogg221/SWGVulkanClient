#include "soe/MessageDispatcher.h"

#include <iomanip>
#include <iostream>

namespace soe {

MessageDispatcher::MessageDispatcher() {
    unknownHandler_ = [](uint32_t hash, PacketBuffer&) {
        std::cout << "Unknown message hash 0x" << std::hex << hash << std::dec
                   << ", ignoring (further occurrences this session are silent)\n";
    };
}

void MessageDispatcher::on(uint32_t hash, Handler handler) {
    handlers_[hash] = std::move(handler);
}

void MessageDispatcher::off(uint32_t hash) {
    handlers_.erase(hash);
}

void MessageDispatcher::onUnknown(UnknownHandler handler) {
    unknownHandler_ = std::move(handler);
    usingDefaultUnknownHandler_ = false;
}

void MessageDispatcher::offUnknown() {
    unknownHandler_ = [](uint32_t hash, PacketBuffer&) {
        std::cout << "Unknown message hash 0x" << std::hex << hash << std::dec
                   << ", ignoring (further occurrences this session are silent)\n";
    };
    usingDefaultUnknownHandler_ = true;
}

void MessageDispatcher::dispatch(const std::vector<uint8_t>& payload) {
    PacketBuffer buf(payload.data(), payload.size());
    buf.readUint16(); // opCount, not currently consumed by any handler
    uint32_t hash = buf.readUint32();

    try {
        auto it = handlers_.find(hash);
        if (it != handlers_.end()) {
            it->second(buf);
        } else if (!usingDefaultUnknownHandler_ || loggedUnknownHashes_.insert(hash).second) {
            unknownHandler_(hash, buf);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to handle message hash 0x" << std::hex << hash << std::dec
                   << ": " << e.what() << "\n";
    }
}

} // namespace soe
