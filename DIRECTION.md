# Direction

This is a short statement of where this project is headed - not a roadmap with
dates, not a promise, and not a commitment to any of it. Things change. Treat it as
"this is the current thinking," nothing more.

## Where it's going

The long-term goal is a complete, modern client capable of real gameplay against a
Core3 server - not just protocol/asset verification tooling. Broadly, in the rough
order things are being tackled:

- **Protocol and world-state correctness first.** The transport layer, login/zone-in
  flow, and object baseline/delta decoding are the foundation everything else sits
  on, and stay the top priority whenever a real gap is found there.
- **Real rendering, incrementally.** Terrain, real object geometry, real character
  models, real building interiors, cell-relative movement/interaction, a full-
  building inspection mode, and real textures on buildings/objects already work
  today (see the README's Current Status). The current focus is refining this
  foundation - real collision (so stairs/ramps and building thresholds work
  naturally), animation playback, and general rendering/lighting quality - until
  it's smooth, reliable, and visually consistent with the live game, followed by an
  efficiency-focused rendering refactor. Gameplay systems (crafting, combat,
  vehicles) are intentionally sequenced AFTER this, not alongside it - moving
  through the world convincingly is the foundation those systems are built on top
  of, not a parallel concern.
- **Gameplay systems, PreCU-targeted.** When combat and profession systems
  eventually get built, the intent is **pre-Combat Upgrade (pre-2005)** authenticity,
  not the NGE (New Game Enhancements) systems that shipped later - a deliberate
  choice, not an oversight.
- **Expansion-era content, decoupled from expansion-era systems.** Later-expansion
  art and planets (Kashyyyk, Mustafar, Hoth, and similar) are genuinely appealing for
  their content and art quality even though their associated NGE-era game systems
  aren't the target - if and when that content gets added, the intent is to use the
  assets without adopting the systems that historically shipped alongside them. This
  is real future work, not close on the horizon.

## What this isn't

This isn't an attempt to perfectly recreate any one specific historical patch/era of
SWG down to the last number. It's a clean-room client built from real protocol facts
and real asset formats, aimed at PreCU-style gameplay with room to selectively pull
in later content on its own terms.

## Pace

This is solo work (with AI assistance - see [`COMMUNITY.md`](COMMUNITY.md)), done at
whatever pace it gets done. There's no timeline, no roadmap with dates, and no
obligation attached to any of the above.
