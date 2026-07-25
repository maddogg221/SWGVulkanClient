# SWG Client New

A modern, from-scratch C++20 client for [SWGEmu](https://www.swgemu.com/) / [Core3](https://github.com/swgemu/Core3) — reimplementing Star Wars Galaxies' network protocol and real client asset formats from first principles, verified continuously against a live server.

This is an **initial public release**: real, buildable source code, so anyone can clone it, build it, and confirm for themselves that this actually works — not a promise, not a demo video, the real thing. See [`COMMUNITY.md`](COMMUNITY.md) for what to expect (and not expect) from this release, and [`LICENSE.md`](LICENSE.md) for the terms you agree to by using it.

## Project Goals

Star Wars Galaxies' original client is closed-source, effectively unmaintainable, and built on nearly 25-year-old technology. Core3, the open-source SWGEmu server emulator, is GPL-licensed — its source is an invaluable protocol *reference*, but not something this project copies from. This project exists to answer a specific question: what would a from-scratch, modern client for this protocol look like, built clean-room and verified against a real server at every step?

Three goals drive the work, in order:

1. **Protocol and world-state correctness.** The transport layer, login/zone-in flow, and object baseline/delta decoding — the plumbing everything else depends on — reimplemented from real wire behavior, not guessed at, and pinned with tests built from real captured traffic wherever practical.
2. **A modern rendering foundation.** A real Vulkan-based renderer capable of rendering the actual game world — real terrain, real buildings, real objects, real characters — accurately and efficiently, as the base for genuinely modern visual quality (lighting, materials, later asset work) rather than a recreation of the original client's own dated rendering approach.
3. **Real gameplay systems**, built on top of a foundation that already works — movement, animation, and rendering need to be solid and consistent with the live game before crafting, combat, and other core systems are layered on top of them.

## Scope

**What this is:**
- An independent, clean-room reimplementation of SWG's network protocol and the real client's own asset formats (meshes, textures, terrain, building layouts, shader/material references), derived directly from protocol facts and real data files, not copied from any reference implementation.
- A real, live 3D client — not just a protocol test harness — with a Vulkan rendering pipeline built from scratch.
- PreCU-era (pre-Combat Upgrade, pre-2005) gameplay authenticity is the long-term target for combat and profession systems, once that work begins — a deliberate choice, not an oversight. See [`DIRECTION.md`](DIRECTION.md) for more on where this is headed and why.

**What this isn't:**
- Not a copy of Core3's code, or any other GPL-licensed project's code. Core3 is read as a reference for protocol *facts*; nothing is copied from it.
- Not a redistribution vehicle for Star Wars Galaxies' own copyrighted assets. This project reads a user's own legitimately-owned client installation locally; it never bundles, hosts, or redistributes any of that content.
- Not a finished game, and not a promise of one on any particular timeline. This is real, working, in-progress software — see Current Status below for what's genuinely done versus still open.
- Not a commercial product, and not licensed for commercial use. See [`LICENSE.md`](LICENSE.md).

## Current Status

**Working today:**
- Full SOE (Sony Online Entertainment) transport layer: session handshake, encryption, integrity checking, compression, and reliable packet delivery.
- The complete login → character-select → zone-in sequence against a live Core3 server.
- A schema-driven decoder for object baselines and deltas (the core of SWG's world-state synchronization) across the majority of SWG's object types.
- A real, live 3D visualizer (`dummyclient --visualize`, Vulkan-based, Windows-only): real procedural terrain, real object/building/character geometry resolved directly from the actual client's own data files, real textures, player-driven WASD movement, and real cell-relative movement/interaction inside player-placed structures (including a full outbound interaction round-trip — e.g. using an elevator and having the server actually move the character to a different floor).
- Real indoor collision: wall blocking, real portal-based room-transition detection, and continuous height-following while climbing or descending real multi-flight staircases and ramps.
- A full-building inspection mode (a free noclip camera showing every real room of a building at once) for visual/scale assessment.
- Real skeletal animation: keyframe playback driven by the client's own gender/mood-aware animation-state selection format, replacing static bind-pose rendering. Standing/idle poses render correctly; the walk cycle has a known, still-open visual defect (see `PROGRESS.md`).
- A real multi-threaded engine loop: networking, asset loading, and rendering each run on their own thread.
- An offline pcap-based decoder for analyzing previously captured sessions.
- A large automated test suite, a substantial fraction of it built on real byte fixtures captured from a live server, not synthetic data alone.

**In progress / deliberately deferred:**
- The walk animation's stride length doesn't yet match the real client (reaches noticeably farther than it should) — narrowed down via direct side-by-side comparison against real official-client footage, see `PROGRESS.md`.
- Real portal-based visibility isn't implemented — a neighboring room visible through an open doorway doesn't render until you actually walk into it.
- Terrain doesn't yet respond to building placement — the ground around a structure's foundation doesn't reflect the grading a real placed building would apply, which is visible right at a building's outer edge.
- Textures are diffuse-only so far; further lighting/material work and general visual polish are still ahead.
- Combat and crafting protocol decode — substantial, largely unexplored territory, intentionally sequenced after the movement/rendering foundation above is solid.

## Architecture Highlights

- **`libs/soe`** — the SOE transport layer: UDP session management, encryption/CRC/compression, and reliable-delivery mechanics, independent of any game-specific logic.
- **`libs/swgproto`** — the SWG application protocol: login/character/zone message flow, and a compile-time schema engine that drives baseline and delta decoding for every object type from declarative field tables.
- **`libs/worldmodel`** — the in-memory Object Model: a persistent, per-object store of live world state, fed by `swgproto`'s decoders.
- **`libs/assets`** — parsers for the real SWG client's own asset formats (archives, object templates, static meshes, skeletal/character meshes, building/portal layouts, shader/material files, textures), zero rendering-API dependency.
- **`libs/terrain`** — a from-scratch reimplementation of SWG's procedural terrain generator, independent of any rendering API.
- **`libs/renderer`** — the Vulkan rendering backend (Windows-only).
- **`libs/clientcommon`** — small shared utilities.
- **`tools/dummyclient`** — a test client exercising the full protocol stack against a real server; also has a live visualizer mode (`--visualize`, Windows-only).
- **`tools/pcapdecoder`** — an offline decoder that replays a captured packet dump through the same pipeline `dummyclient` uses live.

## Design Principles

- **Clean-room, not copy-paste.** Core3's source is read for protocol *facts* and reimplemented independently; its (GPL) code is never copied into this project.
- **The wire is the source of truth — not the source code.** Only real, captured traffic and real client data files confirm what's actually true; documentation and reference source are corroboration, never the final word.
- **Detect and stop, don't guess.** Unknown or ambiguous data produces a clear, logged failure — never a silent misdecode.
- **Verify against real traffic, always.** Decoders and parsers are pinned with permanent tests built from real captured bytes wherever practical.

## Getting Started

See [`HOWTO.md`](HOWTO.md) for build instructions, dependencies, and how to point this at a real server. You'll need your own Core3 server and your own legitimate copy of the original SWG client's data files — this project doesn't provide either.

See [`PROGRESS.md`](PROGRESS.md) for a technical history of how this project got to its current state, phase by phase.

## License

See [`LICENSE.md`](LICENSE.md). Personal, non-commercial use only, provided as-is with no warranty of any kind.
