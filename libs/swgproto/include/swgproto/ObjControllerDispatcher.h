#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjControllerMessage.h"

namespace swgproto {

// Dispatches ObjControllerMessage sub-types to registered handlers by
// header2 ("type"). Mirrors soe::MessageDispatcher's design (on()/off()/
// dispatch()) one level down: MessageDispatcher routes top-level Data
// Channel messages by their 4-byte hash; this routes ObjControllerMessage's
// own payload by its header2 field, parsing the shared envelope
// (header1/header2/objectId/unused) internally before looking up a handler.
//
// Built to eliminate 3 independent hand-written `switch(header2)` copies
// that existed before this class (tools/dummyclient's
// handleObjControllerMessage and handleObjControllerCapture, tools/
// pcapdecoder's handleObjController) - see PHASE_05_STATUS.md for the full
// refactor this is step 1 of.
class ObjControllerDispatcher {
public:
    // Called with the parsed envelope and a buffer positioned right after
    // it (i.e. at the start of that sub-type's own fields) - exactly what
    // every existing per-type parse() function already expects. Reused for
    // both on() and onUnknown() since the envelope (specifically its own
    // header2) already tells a handler what it received, unlike
    // MessageDispatcher's separate UnknownHandler type, which needs the
    // hash passed as an explicit extra parameter instead.
    using Handler =
        std::function<void(const ObjControllerMessage& envelope, soe::PacketBuffer& buf)>;

    // Registers `handler` for `header2`. Registering again for the same
    // header2 replaces the previous handler.
    void on(uint32_t header2, Handler handler);

    // Removes a previously-registered handler for `header2`, if any. A
    // no-op if none was registered.
    void off(uint32_t header2);

    // Overrides the handler called when header2 has no registered handler.
    // Defaults to a silent no-op - deliberately DIFFERENT from
    // soe::MessageDispatcher's default (which logs each distinct unknown
    // hash once): ObjController's "known but not yet decoded" sub-types are
    // numerous and expected (see DISCOVERY.txt's Phase 4 exploration
    // catalog - dozens of real, named sub-types with no decoder yet), so
    // silence matches what every existing caller already did by hand via
    // `default: break;`. A caller that wants to see unknown sub-types (e.g.
    // an exploration pass, matching the temporary hex-dump probes used
    // throughout Phase 4) can still override this.
    void onUnknown(Handler handler);

    // Parses the shared envelope from `buf` (positioned right after
    // opCount+hash, i.e. at ObjControllerMessage's own first field), then
    // dispatches to the handler matching the envelope's header2, or the
    // unknown handler if none is registered (and none is set, this is a
    // silent no-op - see onUnknown() above). Isolates handler exceptions
    // per-call, matching MessageDispatcher's resilience (logged to stderr,
    // not propagated) - envelope parsing itself is NOT wrapped, matching
    // MessageDispatcher's own precedent of trusting opCount+hash framing
    // from a payload that already passed the SOE decrypt/reassembly
    // pipeline.
    void dispatch(soe::PacketBuffer& buf);

private:
    std::unordered_map<uint32_t, Handler> handlers_;
    Handler unknownHandler_; // empty by default - see onUnknown()'s comment
};

} // namespace swgproto
