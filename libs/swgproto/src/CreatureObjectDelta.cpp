#include "swgproto/CreatureObjectDelta.h"

#include <string>

#include "swgproto/CreatureObjectBaseline1.h"
#include "swgproto/CreatureObjectBaseline3.h"
#include "swgproto/CreatureObjectBaseline4.h"
#include "swgproto/CreatureObjectBaseline6.h"
#include "swgproto/SchemaEngine.h"

namespace swgproto {

// Picks the right schema and delegates to the shared engine's
// decodeDeltaMessage() - the per-object-type field-index table this
// function used to hand-maintain (a switch statement restating the SAME
// field order CreatureObjectBaseline1/3's structs already declare) is gone;
// kCreatureObjectBaseline1Schema/kCreatureObjectBaseline3Schema are now the
// only place that order is stated. See PLAN.md's schema-engine plan, Stage 4.
CreatureObjectDeltaDecode decodeCreatureObjectDelta(uint8_t baselineNumber, uint16_t count,
                                                     soe::PacketBuffer& buf) {
    const ObjectSchema* schema = nullptr;
    if (baselineNumber == 1) {
        schema = &kCreatureObjectBaseline1Schema;
    } else if (baselineNumber == 3) {
        schema = &kCreatureObjectBaseline3Schema;
    } else if (baselineNumber == 4) {
        schema = &kCreatureObjectBaseline4Schema;
    } else if (baselineNumber == 6) {
        schema = &kCreatureObjectBaseline6Schema;
    }

    if (schema == nullptr) {
        CreatureObjectDeltaDecode result;
        result.totalCount = count;
        result.stoppedEarly = true;
        result.stopReason = "no schema for CreatureObject baseline " + std::to_string(baselineNumber);
        return result;
    }

    auto generic = decodeDeltaMessage(*schema, count, buf);

    CreatureObjectDeltaDecode result;
    result.updates = std::move(generic.updates);
    result.totalCount = generic.totalCount;
    result.stoppedEarly = generic.stoppedEarly;
    result.stopReason = std::move(generic.stopReason);
    return result;
}

} // namespace swgproto
