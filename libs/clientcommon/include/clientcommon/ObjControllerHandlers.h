#pragma once

#include <functional>
#include <iostream>
#include <string>

#include "swgproto/ObjControllerDispatcher.h"
#include "swgproto/ObjControllerMessage.h"

namespace clientcommon {

// Called after each known sub-type is decoded and printed, with the
// envelope that was just handled - lets a caller layer extra bookkeeping
// (e.g. per-type counts) on top of the shared print logic without
// duplicating it. Most callers don't need this (pass nullptr, the default).
using ObjControllerHook = std::function<void(const swgproto::ObjControllerMessage& envelope)>;

// Registers a decode+print handler on `dispatcher` for every known
// ObjControllerMessage sub-type (Animation, PostureMessage, DataTransform,
// DataTransformWithParent, ShowFlyText, CommandQueueRemove, and any added
// later per PHASE_04_STATUS.md's roadmap) - the SINGLE shared place this now
// lives, replacing what used to be 3 independent hand-written
// switch(header2) copies (tools/dummyclient's handleObjControllerMessage and
// handleObjControllerCapture, tools/pcapdecoder's handleObjController - see
// PHASE_05_STATUS.md for the full refactor).
//
// `prefix` is prepended to every printed line - lets callers keep their
// existing output distinguishable ("ObjController " for the main handler,
// "[CAPTURE] " for interactive capture mode, "" for pcapdecoder) without
// forking the underlying decode/print logic itself.
//
// `onDecoded`, if set, is called after each successful decode+print with the
// envelope - the only current user is interactive capture mode's per-type
// instance counting. Unregistered (unknown) header2 values are NOT covered
// by this function - see ObjControllerDispatcher::onUnknown() for that,
// which stays the caller's own responsibility since "what to do with an
// unknown sub-type" varies more (silent skip vs. hex-dump-and-count) than
// known-type decoding does.
//
// NOTE: this unifies one small pre-existing inconsistency between the 3
// copies it replaces - the main handler's DataTransform/
// DataTransformWithParent lines included a "(idle sync)" annotation
// (added in Phase 4 step 2, see DataTransform.h's own comment on the
// idle-sync finding) that the interactive-capture copy never picked up.
// Standardized on including it everywhere, since it's a real, useful
// clarification, not a data change - flagged here since it's the one place
// this migration's output isn't byte-for-byte identical to every prior
// copy (capture mode's own output was never live-verified before this
// refactor, so the risk is minimal).
void registerObjControllerHandlers(swgproto::ObjControllerDispatcher& dispatcher,
                                    std::ostream& out = std::cout, const std::string& prefix = "",
                                    ObjControllerHook onDecoded = nullptr);

} // namespace clientcommon
