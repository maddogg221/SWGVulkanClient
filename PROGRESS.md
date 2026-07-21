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
