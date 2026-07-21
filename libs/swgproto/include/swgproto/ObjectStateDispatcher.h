#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include "soe/PacketBuffer.h"
#include "swgproto/BaselineEnvelope.h"

namespace swgproto {

// Dispatches BaselinesMessage/DeltasMessage traffic to registered handlers
// keyed on (object type FourCC, baseline number) - e.g. ("CREO", 3),
// ("PLAY", 8), ("TANO", 3). Mirrors soe::MessageDispatcher/
// ObjControllerDispatcher's design one level further down: those route by a
// single integer key; this one's key is a (string, number) pair, since
// BaselinesMessage/DeltasMessage share one envelope shape (BaselineEnvelope)
// across every object type Core3 has.
//
// Replaces the deeply nested if/else-then-switch that used to live inside
// tools/dummyclient's registerObjectTrackingHandlers() - see
// PHASE_05_STATUS.md for the full refactor this is step 4 of.
//
// Self-only guards (e.g. "only handle CREO baseline 3 if this is the local
// player's own character", or "only PLAY baselines belonging to the
// self-attached ghost object") are NOT part of this class's dispatch key -
// they stay inside each registered handler closure, exactly as they did in
// the code this replaces, since "self-only-ness" depends on runtime state
// (the connection's own characterId, or containment tracking) this
// dispatcher has no business knowing about.
class ObjectStateDispatcher {
public:
    // Called with the parsed envelope and a buffer positioned right after
    // it - i.e. at the start of that baseline/delta's own field data.
    // Baseline and delta handlers share this same signature; a delta
    // handler reads the update count from envelope.count, same as the
    // handlers this replaces already did.
    using Handler = std::function<void(const BaselineEnvelope& envelope, soe::PacketBuffer& buf)>;

    // Registers `handler` for baseline messages matching (`objectType`,
    // `baselineNumber`). Registering again for the same key replaces the
    // previous handler.
    void onBaseline(const std::string& objectType, uint8_t baselineNumber, Handler handler);

    // Removes a previously-registered baseline handler for this key, if
    // any. A no-op if none was registered.
    void offBaseline(const std::string& objectType, uint8_t baselineNumber);

    // Overrides the handler called when a valid baseline envelope's
    // (objectType, baselineNumber) has no registered handler. Defaults to a
    // silent no-op, matching every unhandled combination's current
    // behavior (the if/else chain this replaces simply did nothing for
    // anything it didn't explicitly check).
    void onUnknownBaseline(Handler handler);

    // Resets the unknown-baseline handler back to the silent no-op default.
    // Needed by any caller whose ObjectStateDispatcher instance outlives the
    // scope that registered a capturing handler here (e.g. one phase's
    // locally-scoped state) - without this there's no way to avoid leaving
    // a dangling reference behind once that scope ends, since onBaseline's
    // own per-key offBaseline() doesn't cover this handler.
    void offUnknownBaseline();

    // Same as onBaseline()/offBaseline()/onUnknownBaseline(), for
    // DeltasMessage traffic. A separate registry from the baseline one -
    // the same (objectType, baselineNumber) pair often needs genuinely
    // different handling for its baseline vs. its delta (e.g. CREO
    // baseline 3 parses a full struct; CREO delta 3 decodes update
    // entries against a schema).
    void onDelta(const std::string& objectType, uint8_t baselineNumber, Handler handler);
    void offDelta(const std::string& objectType, uint8_t baselineNumber);
    void onUnknownDelta(Handler handler);
    void offUnknownDelta();

    // Parses the shared envelope from `buf` (positioned right after
    // opCount+hash), then dispatches to the matching baseline handler, or
    // the unknown-baseline handler if the envelope parsed but no handler
    // matches its key. A malformed envelope is logged to stderr and
    // dropped (matching BaselinesMessage's prior handling) - this is NOT
    // routed through onUnknownBaseline(), since there's no valid envelope
    // to hand a handler in that case. Isolates handler exceptions,
    // matching every other dispatcher in this codebase. Returns true iff
    // the envelope itself parsed successfully (regardless of whether a
    // handler matched its key) - lets a caller replicate the
    // "only count successfully-framed messages" bookkeeping the code this
    // replaces already did (see tools/dummyclient's ObjectTracker::
    // baselineMessages/deltaMessages).
    bool dispatchBaseline(soe::PacketBuffer& buf);

    // Same as dispatchBaseline(), for DeltasMessage traffic.
    bool dispatchDelta(soe::PacketBuffer& buf);

private:
    using Key = std::pair<std::string, uint8_t>;

    std::map<Key, Handler> baselineHandlers_;
    std::map<Key, Handler> deltaHandlers_;
    Handler unknownBaselineHandler_;
    Handler unknownDeltaHandler_;
};

} // namespace swgproto
