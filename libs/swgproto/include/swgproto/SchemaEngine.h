#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/DeltaFieldUpdate.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// Walks `schema.ancestor` first (if any, decoding into `base +
// schema.ancestorOffset`), then `schema.ownFields` in array order, into
// `base` - the caller guarantees `base` points at a valid, already-
// constructed instance of the schema's struct type. Returns false and
// fills `error` on the first field that fails to decode (matching this
// project's "buffer too short" reporting from every hand-written parser
// this replaces).
bool decodeBaselineInto(const ObjectSchema& schema, void* base, soe::PacketBuffer& buf,
                         std::string& error);

// Thin generic wrapper: allocates a fresh StructT and decodes into it.
// Mirrors the exact ParseResult<T> shape every existing baseline parser
// already returns.
template <typename StructT>
ParseResult<StructT> decodeBaseline(const ObjectSchema& schema, soe::PacketBuffer& buf) {
    StructT result{};
    std::string error;
    if (!decodeBaselineInto(schema, &result, buf, error)) {
        return ParseResult<StructT>::invalid(error);
    }
    return ParseResult<StructT>::ok(std::move(result));
}

// Result of decoding a DeltasMessage's `count` update entries against a
// schema - same shape as the CreatureObjectDeltaDecode/PlayerObjectDeltaDecode
// types it replaces (see CreatureObjectDelta.h/PlayerObjectDelta.h), kept as
// a distinct generic type here since it's genuinely object-type-agnostic.
struct DeltaDecodeResult {
    std::vector<DeltaFieldUpdate> updates;
    uint16_t totalCount = 0; // the envelope's own `count` - may be > updates.size()
    bool stoppedEarly = false;
    std::string stopReason; // populated iff stoppedEarly
};

// Looks up `fieldIndex` in `schema.ownFields`, then `schema.ancestor`'s
// table (recursively) if not found there. A miss anywhere in the chain is
// reported by a null `descriptor` - not an error on its own, but the
// caller (decodeDeltaMessage below) treats a miss as "unknown field index,
// stop", per this project's established detect-and-stop convention for
// genuinely unmapped indices.
struct DeltaFieldLookup {
    const FieldDescriptor* descriptor = nullptr;
};
DeltaFieldLookup findDeltaField(const ObjectSchema& schema, uint16_t fieldIndex);

// The `for (i=0..count) { read index; look up; decode; stop on failure }`
// loop shared by every per-object-type delta decoder so far (see
// CreatureObjectDelta.cpp/PlayerObjectDelta.cpp before their Stage 4/5
// migration) - this is the actual generalization of that loop. Delta stays
// print-only: each field is decoded into a local temporary and formatted,
// never written into a persisted object (see FieldDescriptor::decodeAndFormat).
DeltaDecodeResult decodeDeltaMessage(const ObjectSchema& schema, uint16_t count,
                                      soe::PacketBuffer& buf);

// Like findDeltaField, but also accumulates the byte offset needed to reach
// the found field's owning struct through the ancestor chain - mirrors what
// decodeBaselineInto already does via schema.ancestorOffset when recursing
// for a full baseline decode, just threaded through lookup instead. Needed
// to apply a delta directly onto an ALREADY-CONSTRUCTED instance of the
// composed (derived) struct type: an ancestor-owned field's decodeInto
// trampoline expects `base` to point at the ancestor sub-object, not the
// derived struct's own start address (see ObjectSchema.h's
// decodeIntoTrampoline - it does `static_cast<StructT*>(base)` where
// StructT is the field's OWN declaring struct, not the derived type).
struct DeltaFieldLookupWithOffset {
    const FieldDescriptor* descriptor = nullptr;
    size_t offset = 0;
};
DeltaFieldLookupWithOffset findDeltaFieldWithOffset(const ObjectSchema& schema,
                                                     uint16_t fieldIndex,
                                                     size_t offsetSoFar = 0);

// Result of applying a DeltasMessage's `count` update entries directly onto
// an existing, already-constructed struct instance (`base`), via each
// found field's decodeInto - the write-capable counterpart to
// decodeDeltaMessage's print-only decodeAndFormat path. Same "detect and
// stop on an unmapped index" policy, for the same reason: an unrecoverable
// byte-width mismatch is exactly as fatal for apply as it is for print,
// and guessing past it risks misaligning every field that follows.
struct DeltaApplyResult {
    std::vector<uint16_t> appliedFieldIndices;
    uint16_t totalCount = 0; // the envelope's own `count` - may be > appliedFieldIndices.size()
    bool stoppedEarly = false;
    std::string stopReason; // populated iff stoppedEarly
};

// Applies `count` update entries from `buf` directly onto `base` (a pointer
// to an already-constructed instance of the schema's own struct type - the
// caller owns its lifetime; this never allocates or default-constructs
// anything). Each field's decodeInto always fully REPLACES that field's
// current value (including containers - there is no incremental/patch
// semantics anywhere in this engine, baseline or delta), exactly matching
// what decodeBaselineInto already does for a fresh decode.
DeltaApplyResult applyDeltaMessage(const ObjectSchema& schema, void* base, uint16_t count,
                                    soe::PacketBuffer& buf);

} // namespace swgproto
