#include "swgproto/PlayerObjectDelta.h"

#include <string>

#include "swgproto/PlayerObjectBaseline3.h"
#include "swgproto/PlayerObjectBaseline6.h"
#include "swgproto/SchemaEngine.h"

namespace swgproto {

// Picks the right schema and delegates to the shared engine's
// decodeDeltaMessage() - see CreatureObjectDelta.cpp's Stage 4 migration
// for the identical pattern. The hand-maintained lookupField() switch this
// function used to have is gone; kPlayerObjectBaseline3Schema/
// kPlayerObjectBaseline6Schema are now the only place that field order is
// stated. See PLAN.md's schema-engine plan, Stage 5.
PlayerObjectDeltaDecode decodePlayerObjectDelta(uint8_t baselineNumber, uint16_t count,
                                                 soe::PacketBuffer& buf) {
    const ObjectSchema* schema = nullptr;
    if (baselineNumber == 3) {
        schema = &kPlayerObjectBaseline3Schema;
    } else if (baselineNumber == 6) {
        schema = &kPlayerObjectBaseline6Schema;
    }

    if (schema == nullptr) {
        PlayerObjectDeltaDecode result;
        result.totalCount = count;
        result.stoppedEarly = true;
        result.stopReason = "no schema for PlayerObject baseline " + std::to_string(baselineNumber);
        return result;
    }

    auto generic = decodeDeltaMessage(*schema, count, buf);

    PlayerObjectDeltaDecode result;
    result.updates = std::move(generic.updates);
    result.totalCount = generic.totalCount;
    result.stoppedEarly = generic.stoppedEarly;
    result.stopReason = std::move(generic.stopReason);
    return result;
}

} // namespace swgproto
