#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// Hardcoded literal in Core3's own BaseLineMessage.h (not computed via
// STRING_HASHCODE at that call site) - confirmed in Phase 2 step 1 to also
// equal hashCode("BaselinesMessage").
constexpr uint32_t kBaselinesMessageHash = 0x68A75F0C;

// Hardcoded literal in Core3's own DeltaMessage.h - confirmed in Phase 2
// step 1 to also equal hashCode("DeltasMessage").
constexpr uint32_t kDeltasMessageHash = 0x12862153;

// The common header BaselinesMessage and DeltasMessage both wrap their
// per-object-type field data in - byte-for-byte IDENTICAL shape in both,
// confirmed from reading Core3's own BaseLineMessage.h/DeltaMessage.h
// constructors (both build: opCount + hash + objectId + objectType + a
// "baseline number"/type byte + a size placeholder + a count, then
// per-type data) AND cross-checked against real captured Finalizer bytes.
// See DISCOVERY.txt's "PHASE 2 STEP 3" section for the full derivation.
//
// Field-level decoding of what's INSIDE the `count` entries is
// deliberately out of scope here (PLAN.md's step 4) - it needs
// per-object-type/per-field knowledge (which baseline number maps to which
// class, which field index means what) that this step doesn't have yet.
// This envelope is enough to prove correct framing and track which object
// each baseline/delta belongs to.
struct BaselineEnvelope {
    uint64_t objectId = 0;
    // A 4-character "FourCC" tag identifying the object's base server
    // class (e.g. "PLAY" for PlayerObject, seen in real Finalizer traffic)
    // - see objectTypeTag() to render it.
    uint32_t objectType = 0;
    uint8_t baselineNumber = 0; // which BASE1..BASE9 view/delta group this is
    // Bytes from immediately after this field to the end of the message -
    // i.e. `count`'s 2 bytes plus all the per-type field/update data. Not
    // used to skip anything yet (each dispatched message is already a
    // self-contained payload - see BaselineEnvelope::parse's comment) but
    // useful for sanity-checking framing.
    uint32_t size = 0;
    uint16_t count = 0; // # of baseline fields, or # of delta update entries - not individually decoded yet

    // A pure representation of "these fields were parsed" - no validity
    // state of its own to forget to check. See ParseResult<T> for why
    // parse() returns one of those instead of either a bare BaselineEnvelope
    // (which could be silently invalid) or throwing (which a caller
    // wrapping this in a broad try/catch, as every current caller does,
    // could silently swallow).
    //
    // Parses the fields following opCount+hash. Deliberately does NOT
    // consume the per-type data that follows (the `count` fields/updates) -
    // this message is always a whole, self-contained dispatched payload
    // (see MessageDispatcher::dispatch()), so leaving trailing bytes
    // unread here doesn't risk misaligning any later parse.
    static ParseResult<BaselineEnvelope> parse(soe::PacketBuffer& buf);
};

// Renders `objectType` as its 4-character ASCII tag (e.g. "PLAY"). Falls
// back to a hex string if any byte isn't printable ASCII - a
// malformed/unexpected value should produce a readable diagnostic, not
// garbage or undefined behavior.
std::string objectTypeTag(uint32_t objectType);

} // namespace swgproto
