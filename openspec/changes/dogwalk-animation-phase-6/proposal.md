## Why

Phase 5 shipped the SkeletonModifier **chain** and a LookAt **sample**, but DogWalk still needs product-grade **paper-mouth**, **prop attach**, and configurable **aim/look-at** without opening leash, audio tracks, Tree canvas, or a cutscene director. Grilling locked **DogWalk animation Phase 6** as a hybrid milestone under [ADR 0027](../../../docs/adr/0027-animation-phase-6.md).

## What Changes

- **PaperMouth** ClassDB SkeletonModifier: named jaw bone driven by `openAmount`; optional mode fills the same scalar from Attach occupancy / prop-in-mouth logic.
- **SkeletonAttachModifier**: after sample, copy host attachment bone world transform onto a **child Object** Transform (not cross-Object Skeleton drive).
- **LookAt** elevated from sample to configurable product (bone, target, limits/weight as apply-time fields).
- Scene serialization + Inspector authorship for the three product types; ordered chain preserved.
- Lean **C-ABI / Blunder.Api** bump for key drives only (not full Inspector mirror).
- Edit scrub + lean Play bars for these modifiers only.
- **Done**: engineering gate for the three products + persistence/Inspector + lean ABI + Edit/Play feel. Phase 1–5 open content Done gates remain tracked separately.

**Out of scope:** leash; audio tracks; Cubic/Bezier; AnimationLibrary; Cutscene Director; Tree canvas-as-Done; generic C# SkeletonModifier subclass hot-bridge; remote Skeleton animation drive; closing Phase 1–5 content Done via this phase alone; full Pinda parity as the sole Play bar.

## Capabilities

### New Capabilities
- `skeleton-paper-mouth`: PaperMouth modifier + `openAmount` (+ optional attach-driven fill)
- `skeleton-attach-modifier`: host bone → child Object Transform attach
- `skeleton-look-at-product`: configurable LookAt product (beyond Phase 5 sample)
- `skeleton-modifier-authorship`: scene serialization + Inspector for Phase 6 product modifier types
- `dogwalk-animation-content`: Phase 6 modifier-only Edit/Play Done; parallel with open Phase 1–5 gates

### Modified Capabilities
- `engine-c-abi`: lean entry points for PaperMouth / Attach / LookAt key drives
- `script-native-abi`: Blunder.Api completeness for those lean drives

## Impact

- Engine: three modifier implementations, scene serialize/deserialize, Inspector wiring, Edit preview, lean ABI + Blunder.Api
- Specs: three new capabilities; deltas on skeleton-modifier, dogwalk-animation-content, ABI
- Content: harness + lean Play for mouth / attach / look-at; earlier phase content bars stay tracked
- Docs: CONTEXT Phase 6 terms; ADR 0027
- Depends on: Phase 5 SkeletonModifier chain (main tip)
