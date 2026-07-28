# Development Progress

A technical summary of this project's development, phase by phase. Written for
anyone evaluating the state of the codebase — what's been built, and how it was
verified. See [`DIRECTION.md`](DIRECTION.md) for where the project is headed next,
and the README's Current Status section for what's still open.

Every phase below was verified against a live Core3 server before being considered
complete — this project's core discipline is that real captured/observed traffic and
real client data files are the only source of truth, not documentation or reference
source code.

## Protocol foundation

- **SOE transport layer**: session handshake, XOR encryption, CRC validation, zlib
  compression, and packet fragmentation/reassembly, reimplemented from real observed
  wire behavior.
- **Login → character-select → zone-in**: the complete real sequence against a live
  Core3 server, including the real SOE-level packet-bundling schemes real clients
  use (both the Data Channel bundle marker and the separate MultiPacket mechanism,
  which was initially missed and later found to carry roughly half of a real
  client's outbound traffic).
- **Baseline/delta decoding**: a compile-time schema engine drives decoding of
  object baselines and incremental delta updates — the core of SWG's world-state
  synchronization — across the majority of real object types, with a large
  automated test suite built substantially from real captured byte fixtures rather
  than synthetic data.
- **`ObjControllerMessage` sub-types**: movement, animation, combat feedback, and
  command-acknowledgment messages, prioritized by real observed frequency rather
  than reference-source catalog order.

## Engine and rendering foundation

- **Real procedural terrain**: a from-scratch reimplementation of SWG's classic
  terrain generator (heightfield, color, and shader queries from real per-planet
  data), with chunk meshing and streaming.
- **Player movement**: WASD-driven, camera-relative, locally predicted and clamped
  to real terrain height, reporting real position updates to the server.
- **Multi-threaded engine loop**: networking, asset loading, and rendering each run
  on their own thread, with non-blocking GPU uploads and a real per-frame
  asset-streaming budget.
- **Real character/creature rendering**: real bind-pose 3D models parsed from the
  client's own skeletal-mesh archives (skeleton, skinned mesh, and multi-body-part
  appearance formats), replacing placeholder geometry.
- **Real building rendering**: player-placed structures resolve and render their
  full real geometry, both exterior and every real interior room, via a portal/cell
  asset format entirely distinct from ordinary object geometry.
- **Real cell-relative movement and interaction**: correct rendering while standing
  inside a real room, real containment tracking, real objects placed inside
  buildings (e.g. terminals) rendering in the correct position, and a full outbound
  interaction round-trip — selecting a real interactive object sends the game's real
  interaction message, and the server responds with a genuine state change (verified
  end-to-end using a building's elevator).
- **Full-building inspection mode**: a free noclip camera mode that renders every
  real cell of a building simultaneously, for visual and scale assessment
  independent of the gameplay-accurate single-room rendering used the rest of the
  time.
- **Real textures**: real DDS-format textures (both major compression variants used
  by the client's own files, sampled natively by the GPU) now render on real
  buildings and real objects, resolved through the client's own real shader/material
  reference files rather than flat vertex-color. This pass also found and fixed a
  mesh-parsing defect that had been silently discarding most of the real geometry on
  typical multi-material objects — recovered detail (entrance stairs, trim, panel
  seams) was visible immediately, independent of the texturing work itself.
- **Real indoor collision**: wall blocking, real portal-based room-transition
  detection (replacing an earlier bounding-box approximation), and continuous
  stair/ramp height-following while climbing or descending real multi-flight
  staircases. The floor-height problem in particular required decoding a second,
  separate real navmesh file format distinct from a building's inline collision
  geometry — a real switchback staircase (where a lower flight can share the same
  horizontal footprint as its own upper entrance) is only solvable using that file's
  real per-triangle adjacency data, not a simple raycast or point-in-polygon test.
- **Real skeletal animation**: real keyframe playback replacing the earlier static
  bind-pose rendering, driven by the client's own recursive animation-state
  selection format (a real gender/mood-aware switch/container tree, not a flat clip
  list — an earlier naive "pick the first clip" model could silently select the
  wrong gender or wrong category of animation). A long-standing wrist/finger
  mesh-tearing artifact was resolved: the real quaternion chunks store their
  components in a different order than this project's parser originally assumed,
  a byte-order bug that had survived many prior calibrated-fix attempts because
  bones with symmetric rotation values happened to self-cancel it. The walk cycle
  specifically still has a known, unresolved visual defect — see the process note
  below on numeric-diagnostic debugging for how this is currently being tracked
  down.

## Process notes

A few things about how this project approaches genuinely undocumented territory,
worth stating plainly for anyone evaluating the codebase:

- **No official client source exists.** Every asset and protocol format this
  project has ever gotten right came from a combination of real-bytes probing,
  Core3's own server-side source (for protocol facts only, never copied), and
  occasional corroboration from independent third-party reference implementations
  — never from an authoritative specification, because none exists publicly.
- **Adjacent fixes don't automatically generalize.** More than once, a fix
  confirmed correct for one case (e.g. one asset format, one message type) turned
  out not to cover a related but distinct real case discovered only by testing
  live. Every phase above was independently live-verified for this reason, not
  assumed to inherit correctness from related work.
- **Detect and stop, don't guess.** Unknown or ambiguous wire data and asset bytes
  produce a clear, logged failure in this codebase — never a silent misdecode.
- **When a visual defect resists description, measure it — and then look
  directly.** A live walk-cycle animation defect proved very hard to pin down
  from screenshots and verbal description alone. The first step was live,
  numeric diagnostics computed directly from real skeleton bone positions while
  walking against a real server — signed geometric checks (does a knee bend the
  anatomically correct direction, do the left and right limbs alternate phase, does
  either foot cross the body's own centerline) plus a direct 3D distance
  measurement between corresponding joints. Every one of these came back clean,
  which was itself useful — it ruled out a skeleton-math bug of the same class
  as the earlier quaternion byte-order fix. What finally settled the question was
  building an actual screenshot-comparison capability: a locked-camera-angle
  capture mode added to the client itself, paired with matching frames pulled
  from real official-client footage at the same angles. Side by side, the defect
  turned out to be **stride length**, not a lateral/crossing bug at all — the
  official client's stride is short and contained; this project's reaches
  noticeably farther on both legs and arms, an effect that reads as "crossing"
  in a front-on view purely from perspective. Real per-keyframe rotation
  magnitudes for the leg bones checked out plausible on their own, so the actual
  cause is most likely in how those rotations compose rather than the recorded
  values themselves — still open, but now a specific, narrow target instead of
  an open-ended visual impression.
- **Follow-up: the walk-stride composition target has now been exhausted.** Every
  concrete hypothesis for how the leg rotations combine into a final pose was
  checked directly and came back clean: an alternate, independently-confirmed
  composition formula produces a mathematically identical result for the leg
  chain specifically (the per-bone correction values involved are exactly zero
  there), real bone length stays perfectly constant across a full walk cycle
  with zero stretching, and the clip being played back was confirmed to be the
  correct, standard one rather than an unusually exaggerated variant. Most
  conclusively, an independent, unrelated 3D application was fed the same
  already-decoded keyframe data from a completely fresh starting state — no
  awareness of this project's own code, no shared assumptions — and it
  reproduced the identical stride, matching this project's own computed values
  essentially exactly, bone for bone, frame for frame. That's about as strong a
  signal as this kind of investigation can produce: the recorded animation data
  itself, played back faithfully, genuinely contains this range of motion. The
  stride-length difference is very likely not a bug in this project's own
  reimplementation. Attention has shifted to the resting/idle pose instead,
  which — unlike the legs — does involve real, non-trivial per-bone correction
  values that haven't been tested against an alternate composition approach
  yet.
- **Follow-up: two more resting-pose hypotheses tested and ruled out,
  each with a numeric explanation, not just a screenshot.** Building a
  small automated capture harness (drives the same internal state a human
  tester would, on a timer, rather than needing a live person at the
  keyboard for every comparison) made it possible to A/B two more
  candidate fixes cleanly. First, the per-bone correction value that
  looked promising for the arms turned out to be negligible there too,
  once measured directly — the earlier suspicion that it might matter
  specifically for arms (unlike the legs, where it's exactly zero) didn't
  hold up against the real numbers. Second, every way of ordering how a
  bone's rotation components combine was swept and compared side by side;
  all of them land on essentially the same pose, because one of the real
  correction values involved is large enough to dominate the result
  regardless of ordering. Both are genuine negative results with a
  mechanical reason behind them, not just "still doesn't look right" —
  and, in the process, it became clear that "resting" isn't actually a
  bare, unanimated pose at all: the character is always playing a real,
  low-amplitude idle animation, even standing still, which reframes what
  the remaining open question even is.
- **Follow-up: the resting-pose formula and correction values confirmed
  correct against the real client's own leaked production source — and
  a real, previously-unread animation-clip chunk found and fixed along
  the way.** Genuine leaked source for the real client's own runtime
  pose-computation code became available and was used to check several
  standing assumptions directly rather than continuing to infer from
  decoded file data alone. The formula this project already uses to
  combine a bone's rest-orientation corrections with its animated
  rotation is character-for-character identical to the real client's
  own; the correction values themselves are confirmed genuine,
  deliberately-authored rig data, not a decode artifact. Neither is the
  cause. Fresh, deliberate side/back/front comparison screenshots
  against the real client confirmed the resting stance really is a
  continuous idle-gesture loop (tapping a foot, glancing around) —
  but arm/leg placement reads as consistently wrong regardless of which
  gesture is playing, ruling out a timing mismatch as the explanation.
  Reading the real client's own skeleton-file loading code directly,
  chunk by chunk, found that every piece of data that file format can
  contain is already read and used by this project — the format itself
  is now fully exhausted, not just deprioritized. That same pass
  surfaced a genuinely new, previously-unread piece of the *animation
  clip* format instead: a bone with no explicit animated motion in a
  given clip doesn't default to "unchanged," as this project had
  assumed — the format stores a real, fixed, deliberately-authored
  resting value for it. That gap was closed (decoded, wired in, covered
  by new automated tests against real content, live-verified end to
  end) — a genuine, previously-missing piece of real per-character
  animation data, correctly restored for the first time. **It did not
  fix the resting-pose difference.** The newly-restored values are, like
  the animated motion measured earlier, only a few tens of degrees at
  most — real and correct, but still nowhere near the roughly
  quarter-turn a shoulder would need to travel to reach a natural
  at-the-sides stance from this project's own computed rest
  configuration. Both relevant file formats are now genuinely exhausted;
  whatever explains the real client's actual resting configuration
  isn't per-clip animation data at all. This is, by a clear margin, the
  most thoroughly investigated open defect in this project's history,
  and the internally-visible leads are exhausted — if this describes a
  symptom you recognize from real hands-on experience with this
  platform's animation pipeline, please open an issue.
- **Follow-up: isolating the pose back to zero animation confirms the
  base rest configuration itself is correct — narrowing the question to
  one specific, well-defined remaining possibility.** With every per-clip
  animation data source exhausted (see above), the next step was to
  strip animation out of the equation entirely and look at the raw,
  computed base pose alone — the thing every animation is layered on top
  of. Doing that and comparing it from every angle confirmed it's a
  genuine, textbook T-pose: arms held fully horizontal. That's actually a
  clarifying result, not a new problem — a T-pose base reference is the
  standard convention for a skinned character rig, used essentially
  universally, so this project's own base pose is very likely correct as
  it stands. The real client never shows literal base pose either; a
  character is always animated. Which means the entire job of turning
  that T-pose into a natural at-the-sides stance has to come from
  animation data layered on top of it — and every real source of that
  data available to this project, exhaustively checked, tops out around
  a third of what's actually needed. That points the remaining
  investigation at one specific, well-defined question instead of an
  open-ended one: either this project is resolving to the wrong specific
  clip for this situation, or the real client applies some additional,
  always-on base layer before the visible idle motion that hasn't been
  identified yet. Settling that needs a form of investigation this
  project has used before but not yet applied here — directly observing
  the real client's own behavior at the machine-code level, a
  higher-cost, higher-risk step than anything used to reach this point.
  Given how much has already gone into isolating this, this is a
  deliberate, natural point to also step back and let this stand as a
  clearly documented open question while the project's attention moves
  toward other, higher-priority systems (crafting, combat) for now —
  revisited if new information (either from further investigation or
  from someone recognizing this symptom) makes the next step clearer.
