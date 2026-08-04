# Task 2.1 Report — PaperMouth applies jaw pose from openAmount

## Status
**Done** — TDD GREEN on `feat/dogwalk-animation-phase-6`.

## Deliverables
- `SkeletonPaperMouthModifier` (`skeleton_paper_mouth_modifier.h/.cpp`): applies local jaw rotation from clamped `openAmount` (0..1) on default bone `"Jaw"`; 45° max open on local +X.
- ClassDB product **`PaperMouth`** with `open_amount` and `bone_name` properties (`skeleton_paper_mouth_modifier_registration.cpp`).
- `Object::addSkeletonPaperMouthModifier()` factory.
- Wired into `class_db.cpp`, `register_generated.h`, `engine/src/runtime/CMakeLists.txt`.
- Test: `dogwalk_phase6_paper_mouth_test.cpp` — closed vs open `openAmount` yields detectable jaw quaternion change; ClassDB round-trip.

## Test
```
dogwalk_phase6_paper_mouth_test: all passed
```

## Out of scope (deferred)
- 2.2 configurable jaw bone (bone_name registered; default `"Jaw"` only exercised)
- 2.3 attach-driven mode
- 2.4 Edit scrub API
- C-ABI / Blunder.Api / scene serialize / Inspector

## Concerns
- Jaw open axis is fixed local +X at 45° max; content rigs may need 2.2 bone config or axis tuning.
- ClassDB name is `PaperMouth` (spec) while C++ type is `SkeletonPaperMouthModifier` (LookAt naming mirror).
