#include "worldmodel/SingletonRegistry.h"

#include <iostream>

namespace worldmodel {

void SingletonRegistry::registerHandlers(swgproto::ObjectStateDispatcher& dispatcher) {
    dispatcher.onBaseline(
        "GILD", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            auto gild3 = swgproto::GuildObjectBaseline3::parse(buf);
            if (!gild3.ok()) {
                std::cerr << "SingletonRegistry: failed to decode GuildObject BASE3: "
                           << gild3.error() << "\n";
                return;
            }
            if (!guildDirectory_.has_value()) {
                guildDirectory_ = GuildDirectory{};
            }
            guildDirectory_->managerObjectId = env.objectId;
            guildDirectory_->base3 = gild3.value();
            guildDirectory_->lastUpdate = std::chrono::steady_clock::now();
        });
    dispatcher.onBaseline(
        "GILD", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            auto gild6 = swgproto::GuildObjectBaseline6::parse(buf);
            if (!gild6.ok()) {
                std::cerr << "SingletonRegistry: failed to decode GuildObject BASE6: "
                           << gild6.error() << "\n";
                return;
            }
            if (!guildDirectory_.has_value()) {
                guildDirectory_ = GuildDirectory{};
            }
            guildDirectory_->managerObjectId = env.objectId;
            guildDirectory_->base6 = gild6.value();
            guildDirectory_->lastUpdate = std::chrono::steady_clock::now();
        });
}

} // namespace worldmodel
