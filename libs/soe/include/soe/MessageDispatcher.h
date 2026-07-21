#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "soe/PacketBuffer.h"

namespace soe {

// Dispatches received Data Channel message payloads (opCount+hash+fields,
// as returned by SoeSession::receiveMessages()) to registered handlers by
// message hash. Replaces the hand-copied "read opCount+hash, if/else on
// hash" blocks that used to be duplicated per receive loop in dummyclient.
//
// Isolates exceptions per-message: a malformed/truncated payload is logged
// and dropped rather than propagating and aborting whatever loop called
// dispatch(), matching the resilience SoeSession itself already applies at
// the transport layer (one bad packet shouldn't kill the whole session).
class MessageDispatcher {
public:
    using Handler = std::function<void(PacketBuffer&)>;
    using UnknownHandler = std::function<void(uint32_t hash, PacketBuffer&)>;

    MessageDispatcher();

    // Registers `handler` for `hash`, called with the buffer positioned
    // right after opCount+hash. Registering again for the same hash
    // replaces the previous handler.
    void on(uint32_t hash, Handler handler);

    // Removes a previously-registered handler for `hash`, if any. A no-op
    // if none was registered. Meant for a dispatcher shared across several
    // sequential phases of the same session (see tools/dummyclient's
    // authenticateZoneSession/zoneInAsCharacter/createCharacterHeadless,
    // which all register/pump/off() their own handlers on one shared
    // per-session dispatcher): a phase's handler typically captures that
    // phase's local "done" flags by reference, which stop being valid once
    // the phase function returns - off() lets the phase clean up its own
    // registration before returning, so the dispatcher can't dispatch to a
    // dangling capture if that same hash's message ever arrives again
    // during a later phase.
    void off(uint32_t hash);

    // Overrides the handler called when a message's hash has no
    // registered handler. Defaults to logging "Unknown message hash
    // 0x..., ignoring" to stdout the FIRST time each distinct hash is seen,
    // then staying silent for repeats (a populated zone can otherwise
    // produce thousands of repeats of the same few genuinely-unhandled
    // hashes - see DISCOVERY.txt). This dedup applies only to the default
    // handler; a caller-supplied one (via this method) is always called in
    // full, un-deduped, since the caller explicitly asked to see every
    // occurrence.
    void onUnknown(UnknownHandler handler);

    // Resets the unknown-message handler back to the default logged-once
    // behavior. Needed by any caller whose custom onUnknown() handler
    // captures state by reference that doesn't outlive the scope that
    // registered it (e.g. a bounded command-capture window) - mirrors
    // swgproto::ObjectStateDispatcher's offUnknownBaseline()/offUnknownDelta(),
    // added for the exact same dangling-reference reason.
    void offUnknown();

    // Parses opCount+hash from `payload`, then dispatches to the matching
    // handler (or the unknown-handler). Catches and logs (to stderr) any
    // exception the handler throws, rather than propagating it.
    void dispatch(const std::vector<uint8_t>& payload);

private:
    std::unordered_map<uint32_t, Handler> handlers_;
    UnknownHandler unknownHandler_;
    bool usingDefaultUnknownHandler_ = true;
    std::unordered_set<uint32_t> loggedUnknownHashes_; // only consulted while using the default handler
};

} // namespace soe
