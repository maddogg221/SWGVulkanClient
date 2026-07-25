# SWG Blender Toolkit - build notes

Started 2026-07-24, during the Phase 21 finger/wrist "sail" artifact
investigation. Two purposes, deliberately combined rather than kept
separate:

1. **Immediate**: build an independent reference implementation of the
   skeletal animation *composition* step (hierarchy walk + rotation
   composition + skinning), using Blender's own mature, unrelated-to-our-
   code armature/animation system, to test a specific question our own
   live x32dbg RE and code audit couldn't conclusively answer: is the
   wrist/finger tearing bug in this project's own composition math, or in
   the underlying decoded data itself? Feed the same already-decoded data
   into an independent system and see whether it reproduces the same tear.
2. **Long-term**: the seed of a real, permanent tool for this project - a
   menu-driven Blender addon capable of importing `.tre` archives directly,
   plus meshes, skeletons, animations, and textures, with a real UI. This is
   explicitly NOT meant to be a throwaway diagnostic script - it's built as
   a proper Blender addon from the start so tonight's bare-minimum work is
   already sitting in the right shape to grow.

See [[project_engine_vision_and_asset_ceiling]] (private memory) for why
this kind of tooling matters to the project's own long-term vision -
independent of tonight's specific diagnostic use, this is real
infrastructure for the "community can build on this" goal.

## Why not reimplement `.skt`/`.ans` byte-level parsing in Python

This project's own C++ parsers (`libs/assets/src/Skeleton.cpp`,
`libs/assets/src/AnimationClip.cpp`) already correctly parse these formats -
that was never in question tonight, only the *composition* math applied
afterward (hierarchy walk, quaternion composition, skinning) was suspect.
Reimplementing the raw byte-level decode a second time in Python would risk
silently diverging from the confirmed-correct C++ decode, and wouldn't
actually test the thing we need to test.

Instead: a new C++ diagnostic dumps the *already-decoded* data (bone
hierarchy + bind pose + per-frame keyframe rotations for one specific clip)
to a plain JSON file. Blender then only has to build an Armature + Action
from already-correct numbers and let its own independent composition/
skinning code do the rest. This cleanly isolates the one variable actually
in question.

## Architecture (as of tonight, bare minimum)

```
tools/swg_blender_toolkit/
  NOTES.md                      - this file
  swg_blender_toolkit/           - the actual installable Blender addon package
    __init__.py                  - bl_info, registration, minimal N-panel UI
    skeleton_import.py           - reads the JSON dump, builds a real Armature
    animation_import.py          - reads the JSON dump, builds a real Action
                                    (quaternion rotation F-curves per bone)
    mesh_bridge.py                - imports the .mgn mesh via the existing
                                    community io_scene_swg_mgn addon (a real
                                    dependency, not reimplemented), then adds
                                    an Armature modifier binding it to the
                                    imported skeleton
    compare.py                    - extracts Blender's own computed world
                                    bone matrices at a given frame, writes
                                    them out for a direct numeric diff
                                    against this project's own C++ values
                                    at the same frame - not just an eyeball
                                    visual check
```

Registered as real Blender operators (not a `--python` throwaway script),
so Blender's own operator search (F3) already works, and a minimal sidebar
(N-panel) with one button per step exists from the start - the natural seed
for a fuller custom UI later, not something bolted on afterward.

## What's deliberately NOT built tonight (bare minimum first, per explicit
## instruction), but planned for and should not be architecturally blocked

- **Native `.tre` archive reading in Python** - tonight's version relies on
  files already extracted to disk via this project's own `dummyclient
  --extract-raw` CLI flag (a real, working, already-tested mechanism, just
  requiring the C++ project to be built). A native Python `.tre` reader
  would let the toolkit work standalone, without needing this project's own
  C++ build at all - important for the "easy to use" long-term goal, since
  a modder shouldn't need to compile a whole client to open an archive.
  `.tre` is a documented, known format - real, scoped, doable work, just not
  tonight's bare minimum.
- **Texture import** (`.dds` via `.sht` shader files) - not needed to
  answer tonight's specific composition question.
- **A fuller menu/wizard UI** - tonight's panel is intentionally minimal
  (one button per step, in order). A more polished multi-step UI, real
  file browsers instead of hardcoded paths, and error handling for a
  non-developer user are real future work.

## Build log

### 2026-07-24 - bare minimum built and working end to end

Built in this order: `directxmath_compat.py` (ported DirectXMath's exact
`XMQuaternionMultiply` formula, not trusting `mathutils`'s own convention -
see its own docstring for why this matters), `skeleton_import.py`
(Armature builder using the "identical trivial rest bone" trick so pose-
space carries 100% of the real transform), `animation_import.py`
(keyframes real decoded rotations using the ported formula), `mesh_bridge.py`
(thin wrapper around the existing `io_scene_swg_mgn` community addon, adds
an Armature modifier), `compare.py` (extracts Blender's own world bone
matrices at an exact frame for numeric diffing). New C++ side: `dummyclient
--dump-anim-skeleton/--dump-anim-clip/--dump-anim-json-out` dumps already-
decoded data; a small standalone `compare_worldpos.cpp` (not checked into
the repo, scratchpad only so far) calls this project's own
`sampleLocalBoneTransforms`/`computeWorldBoneTransforms` directly for an
exact-frame comparison, avoiding live-timing imprecision.

Full pipeline (skeleton -> animation -> mesh+bind -> compare) confirmed
working headlessly. First real result: root/spine/clavicle world
transforms match Blender exactly; the per-bone LOCAL rotation composition
formula was independently verified mathematically correct (matches to an
overall quaternion sign, which is a real no-op - double cover). The actual
divergence is isolated to the local-to-world combination step or a
coordinate-handedness mismatch, starting at the first bone with real
animation data. See [[project_animation_phase21_inprogress]] (private
memory) for the full numeric trail.

**Known gaps for next time**: translation-channel animation isn't wired
into the Blender importer yet (rotation-only v1); `compare_worldpos.cpp`
needs a permanent home in the repo (currently scratchpad-only); the
handedness/world-composition hypothesis itself is not yet resolved.
