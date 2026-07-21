#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "swgproto/GuildObjectBaseline3.h"
#include "swgproto/GuildObjectBaseline6.h"
#include "swgproto/ObjectStateDispatcher.h"

namespace worldmodel {

// GuildObject is confirmed NOT a per-instance world object -
// GuildObjectImplementation::sendBaselinesTo() is dead code; real GILD
// traffic is a zone-wide GuildManagerImplementation singleton's directory,
// sent to every player at login under the MANAGER's own object ID (see
// GuildObjectBaseline3.h). Forcing it into ObjectStore's per-objectId map
// would misrepresent it as "one guild-shaped world object," which it
// categorically isn't - so it lives here instead, structurally separate.
// One optional slot per baseline number, same mechanical rule ObjectStore's
// own types follow - a directory update is either present or absent per
// baseline number, not an all-or-nothing struct.
struct GuildDirectory {
    uint64_t managerObjectId = 0; // the GuildManager's own object id - NOT a per-guild id
    std::optional<swgproto::GuildObjectBaseline3> base3;
    std::optional<swgproto::GuildObjectBaseline6> base6;
    std::chrono::steady_clock::time_point lastUpdate{};
};

// Home for GuildObject and any future confirmed singleton/service type this
// project decodes - kept generic (not GuildRegistry) rather than assuming
// GuildObject stays the only one. GILD traffic never enters
// ObjectStore/ObjectVariant at all: this class independently claims
// ("GILD", 3) and ("GILD", 6) on the same ObjectStateDispatcher ObjectStore
// registers onto. No delta registration for either - no GuildObjectDeltaMessage6.h
// exists in source at all, and BASE3's own guildList delta (index 0x04) is a
// nested ADD/REMOVE/REMOVE_ALL list-command shape deliberately deferred
// (see GuildObjectBaseline3.h).
class SingletonRegistry {
public:
    void registerHandlers(swgproto::ObjectStateDispatcher& dispatcher);

    const GuildDirectory* guildDirectory() const {
        return guildDirectory_.has_value() ? &guildDirectory_.value() : nullptr;
    }

private:
    std::optional<GuildDirectory> guildDirectory_;
};

} // namespace worldmodel
