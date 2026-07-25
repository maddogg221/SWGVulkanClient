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
