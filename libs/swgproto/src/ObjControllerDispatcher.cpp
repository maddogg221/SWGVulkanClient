#include "swgproto/ObjControllerDispatcher.h"

#include <iostream>

namespace swgproto {

void ObjControllerDispatcher::on(uint32_t header2, Handler handler) {
    handlers_[header2] = std::move(handler);
}

void ObjControllerDispatcher::off(uint32_t header2) {
    handlers_.erase(header2);
}

void ObjControllerDispatcher::onUnknown(Handler handler) {
    unknownHandler_ = std::move(handler);
}

void ObjControllerDispatcher::dispatch(soe::PacketBuffer& buf) {
    auto envelope = ObjControllerMessage::parse(buf);

    try {
        auto it = handlers_.find(envelope.header2);
        if (it != handlers_.end()) {
            it->second(envelope, buf);
        } else if (unknownHandler_) {
            unknownHandler_(envelope, buf);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to handle ObjControllerMessage header2=0x" << std::hex
                   << envelope.header2 << std::dec << ": " << e.what() << "\n";
    }
}

} // namespace swgproto
