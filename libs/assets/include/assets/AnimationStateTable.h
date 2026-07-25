#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

// Real `.lat` states are NOT simple flat clip lists - a real state is a
// recursive selection tree (confirmed 2026-07-25 by dumping the full,
// unflattened real structure of `loop_standing`, one of the most complex
// real states in the table): a top-level real `FORM SSAT` switches on a
// named parameter (confirmed real: "gender", picking between two real
// `FORM SPAT` branches), each branch has its own nested `FORM SSAT`
// switching on ANOTHER named parameter (confirmed real: "mood"), whose
// options are real `FORM AGAT` "ambient variant" entries - each carrying a
// real list of trigger names (`CHNK ACTN`, e.g. "nervous_ambient_01_ag",
// "ag_sitting_chair_m_1") plus a real min/max duration pair (`CHNK INFO`,
// two floats) - wrapping one real clip via `FORM LOOP`. A `FORM SPAT`
// branch also carries real locomotion-tagged sibling clips (plain
// `FORM PXAT` with a real `CHNK PUNF` = "locomotion") alongside its mood
// tree, not inside it.
//
// This tree preserves that real structure instead of flattening it, so a
// real selection can be made (by known gender, and a sensible untriggered
// default for the mood axis) instead of guessing "first clip found
// anywhere" - which the previous flattened representation did, and which
// could just as easily return a clip from the WRONG gender branch
// depending on real file traversal order, not just the wrong mood.
enum class AnimationNodeKind {
    // A real leaf clip (`FORM PXAT`). `clipPath` is its own real `.ans`
    // path; `punfParameterName` is the first string of its real, optional
    // `CHNK PUNF` pair (confirmed real value seen so far: "locomotion" -
    // marks this clip as belonging to a different selection axis than the
    // sibling mood tree, e.g. the real walk/run clips alongside a
    // standing state's own idle variants), empty if this clip has no
    // `PUNF` at all (the common case).
    Clip,
    // A real named N-way switch (`FORM SSAT` -> `FORM ANMS`).
    // `parameterName` is the switch's own real parameter name (confirmed
    // real values: "gender", "mood" - genuinely just a string, no fixed
    // enum of real names is assumed); `children` holds one real node per
    // real ANMS option, in real file order.
    Switch,
    // A real, unnamed container list (`FORM SPAT`'s own top-level
    // children, skipping its own leading 1-byte `CHNK INFO` whose meaning
    // isn't decoded yet) - a real mix of exactly one `Switch` (the mood
    // tree) and zero or more `Clip` nodes (real locomotion siblings, or
    // occasionally other direct clips) found at the same real level, not
    // itself switched on anything.
    Container,
    // A real ambient variant (`FORM AGAT`). `triggerNames` is its real
    // `CHNK ACTN` name list (empty if this variant has no gating trigger
    // at all - a real, always-eligible "default" ambient filler, e.g. the
    // real `fishing_idle1`-style bare PXAT siblings some mood lists also
    // contain, though those parse as plain `Clip` nodes, not `Variant` -
    // this kind only covers the real AGAT-wrapped case).
    // `minDurationSeconds`/`maxDurationSeconds` are AGAT's own real
    // `CHNK INFO` float pair - not consumed by selection yet, kept for a
    // future ambient-timing pass. `children` holds exactly one node: the
    // real clip this variant wraps via `FORM LOOP` (unwrapped here - LOOP
    // itself carries no real data of its own beyond being a wrapper).
    Variant,
};

struct AnimationNode {
    AnimationNodeKind kind = AnimationNodeKind::Clip;

    std::string clipPath;           // Clip only
    std::string punfParameterName;  // Clip only, empty if this clip has no real PUNF

    std::string parameterName;  // Switch only

    std::vector<std::string> triggerNames;  // Variant only, real ACTN names
    float minDurationSeconds = 0.0f;        // Variant only
    float maxDurationSeconds = 0.0f;        // Variant only

    // Switch: one real child per ANMS option. Container: every real
    // top-level child found under the SPAT. Variant: exactly one (the
    // wrapped clip). Clip: always empty.
    std::vector<AnimationNode> children;
};

// One real named animation state from a `.lat` file - e.g.
// "loop_combat_standing", "trn_prone_to_standing". `root` is the state's
// own real top-level node - for the common case (most real states checked
// so far - simple named transitions, single combat moves) this is
// directly a `Clip` node; for a real multi-variant state it's a `Switch`
// or `Container` as described above.
struct AnimationState {
    std::string name;
    AnimationNode root;
};

// Plain data from a real ".lat" file (FORM LATT) - a per-skeleton table of
// every named real animation state (bind poses, loops, and transitions),
// resolved via a real skeletal appearance's LATX chunk (see
// SkeletalAppearance.h). Real structure confirmed by dumping an actual
// extracted .lat: FORM(LATT) -> FORM(0000) -> CHNK INFO (own ash filename,
// NUL-terminated, + a trailing uint16 real state count - not otherwise
// consumed) followed by `numStates` real FORM(ANIM) children, each
// CHNK INFO (the state's own name, NUL-terminated, no count prefix) + one
// real node tree as described on AnimationNode above.
struct AnimationStateTableData {
    std::vector<AnimationState> states;
};

// What to select with, given a real state's node tree. `gender` should be
// one of "male"/"female" (case-insensitive substring match against a real
// Switch's own real ANMS-order children - see AnimationStateTable.cpp's
// own comment on why file order, not a decoded value table, is used to
// tell the branches apart) - any other value falls back to the first
// child. There is deliberately no "mood"/trigger context yet: this
// project does not track real server-driven mood/emote state, so any
// `Switch` whose `parameterName` isn't "gender" (confirmed real: "mood",
// but treated generically - ANY unrecognized switch name gets the same
// treatment) picks a stable, sensible untriggered default instead of
// modeling the real trigger-matching system.
struct AnimationSelectionContext {
    std::string gender;
};

class AnimationStateTable {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected.
    static AnimationStateTableData parse(const std::vector<uint8_t>& bytes);
};

// Walks `root` per `context` and returns the final real clip path selected
// - empty if the tree contains no reachable real Clip node (shouldn't
// happen for a well-formed real state, but tolerated rather than thrown,
// matching this project's "a missing/malformed piece of content degrades
// gracefully" convention elsewhere).
//
// `preferLocomotion` - when true, actively prefers a real `Clip` node
// whose `punfParameterName == "locomotion"` over the state's own mood
// tree, when both are siblings under the same real `Container` (the real
// shape of states like "loop_combat_standing", which bundle real walk/run
// clips alongside their own idle-variant tree) - used for real movement
// states; false (the default) skips locomotion-tagged clips entirely,
// used for a real resting/idle selection.
std::string selectAnimationClip(const AnimationNode& root, const AnimationSelectionContext& context,
                                 bool preferLocomotion = false);

} // namespace assets
