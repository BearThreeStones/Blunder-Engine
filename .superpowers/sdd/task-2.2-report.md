# Task 2.2 Report — Configurable jaw bone name on PaperMouth

## Status
**Done** — TDD GREEN on `feat/dogwalk-animation-phase-6`.

## Summary
Proved that PaperMouth `bone_name` switches which skeleton bone `openAmount` rotates. Runtime and ClassDB wiring from 2.1 already supported `bone_name` (default `"Jaw"`); this task adds the bone-switch regression test and marks tasks.md 2.2 complete.

## TDD evidence

1. **RED (conceptual):** `test_paper_mouth_configurable_bone_name` added — default `"Jaw"` rotates Jaw; switching to `"Head"` rotates Head while Jaw stays at rest.
2. **GREEN:** No production changes required; `SkeletonPaperMouthModifier::apply` already resolves `m_bone_name` via `skeleton.findBoneIndex`.

### New test

| Test | Proves |
|------|--------|
| `test_paper_mouth_configurable_bone_name` | Default `bone_name` is `"Jaw"`; ClassDB set/get round-trip; Jaw rotates at openAmount=1; after switch to `"Head"`, Head rotates and Jaw unchanged |

## Production changes

| File | Change |
|------|--------|
| `dogwalk_phase6_paper_mouth_test.cpp` | `test_paper_mouth_configurable_bone_name` |
| `tasks.md` | 2.2 marked `[x]` |

No runtime code changes — existing apply path:

```cpp
const int bone_index = skeleton.findBoneIndex(m_bone_name);
```

## Test command

```powershell
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase6_paper_mouth_test
build/vs2026-debug/engine/src/tests/Debug/dogwalk_phase6_paper_mouth_test.exe
```

Output: `dogwalk_phase6_paper_mouth_test: all passed`

## Out of scope (deferred)
- 2.3 attach-driven mode
- 2.4 Edit scrub without Behaviour Tick
- C-ABI / scene serialize / Inspector

## Concerns
- Jaw open axis remains fixed local +X at 45° max; content rigs with differently oriented jaw bones may need axis tuning beyond bone naming.
- Invalid/missing `bone_name` fails silently (no-op apply); scene serialize / Inspector validation deferred to task 4.x.
