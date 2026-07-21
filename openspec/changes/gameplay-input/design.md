## Context

Play Mode runs Project scripts inside **`engine_player`** (ADR 0014). Behaviours Tick via CoreCLR + registered **`BlunderNativeAbi`**. Glossary and ADR 0015 lock product **Gameplay Input**: authoritative only in Player; Behaviours read named **Gameplay Actions**; first slice is Move (2D) + Jump (press-edge); Pause discards edges; unfocused Player is idle; static `Input` on `Blunder.Api`; extend existing NativeAbi (no second Input ABI).

Today, native `InputSystem` builds a legacy **`GameCommand`** bitfield used by editor/camera paths. That is not the product Action API and must stay separate.

Coordinate system: Z-up, Move `(x,y)` → world **+X / +Y** (horizontal plane).

## Goals / Non-Goals

**Goals:**
- Sample WASD → normalized Move and Space → shared Jump edge once per sim frame in Player
- Publish Actions through new NativeAbi / C-ABI entry points into `Blunder.Api.Input`
- Idle Actions when not Player host, when Play Pause is active, or when Player window lacks OS focus
- Keep Jump edge identical for every Behaviour that polls in the same Tick

**Non-Goals:**
- Remapping, gamepad, raw KeyCode as primary Behaviour API
- Edit Mode / editor window as authoritative Gameplay Input
- Buffering Pause-time Jump into the first Resume Tick
- Separate Input-only ABI registration or ClassDB properties for input
- DogWalk content / locomotion tuning

## Decisions

### D1 — Extend existing NativeAbi (not a second table)
**Choice:** Append `gameplay_input_get_move` and `gameplay_input_was_jump_pressed` (names finalized in tasks) to `BlunderNativeAbi`; bump `BLUNDER_ENGINE_C_ABI_VERSION` to **3**.
**Why:** Same registration path as Object/lifecycle; one ObjectDB / one table. Rejected: separate `RegisterGameplayInputAbi`.
**Alternative considered:** DllImport-only input DLL — rejected (second image / split brain).

### D2 — Player sampler owns Action frame; editor `GameCommand` untouched
**Choice:** New `GameplayInput` (or equivalent) module samples SDL keyboard for Actions; `InputSystem`/`GameCommand` remain for editor camera and are not the managed product surface.
**Why:** Avoid name collision and accidental coupling of editor shortcuts to Behaviours.
**Alternative considered:** Reinterpret `GameCommand` bits as Actions — rejected (CONTEXT avoid list).

### D3 — Sample once before Behaviour Tick; shared edge
**Choice:** Each non-deferred frame in Player: update Action snapshot, then run lifecycle Tick. Jump is true for the whole frame if Space transitioned down-since-last-sample while authoritative.
**Why:** All Behaviours see the same Jump; no consumer latch.
**Alternative considered:** First-reader-consumes — rejected.

### D4 — Pause and focus force idle + sync edge baseline
**Choice:** While Pause or unfocused (or non-Player): Move = (0,0), Jump = false; update “Space was down” to current key state without emitting an edge.
**Why:** Matches grilled Pause discard and unfocused idle; holding Space across Pause/Resume does not fire Jump.
**Alternative considered:** Queue Jump during Pause — rejected.

### D5 — Managed static `Input` façade
**Choice:** `Blunder.Input.GetMove()` → `(float x, float y)` or `Vec2`; `Blunder.Input.WasJumpPressed()` → `bool`. Calls NativeAbi pointers only.
**Why:** Matches Unity-ish poll style without hanging state on Object.
**Alternative considered:** Object properties / ClassDB — rejected.

### D6 — Default binding math
**Choice:** W/S → ±Move.y, A/D → ±Move.x; opposing cancel; diagonal normalize so length ≤ 1; Space press-edge → Jump.
**Why:** DogWalk starter; YAGNI remapping.

## Risks / Trade-offs

- [ABI layout break] Managed `NativeAbi` / native fill / tests must update together → Mitigation: version 3 + sizeof/completeness tests fail closed
- [Editor vs Player confusion] Devs may poll `Input` in Edit Mode ScriptHost and see idle → Mitigation: docs + idle return; product path is Player only
- [SDL keyboard only] No gamepad yet → Mitigation: explicit Non-Goal; sampler interface can grow later
- [Double sampling] `InputSystem` still ticks in Player → Mitigation: leave it; Actions do not read `GameCommand`

## Migration Plan

1. Land native sampler + C-ABI + tests
2. Bump NativeAbi managed + ScriptHost registration completeness
3. Ship `Input` façade + managed tests
4. Wire Player frame sample before Tick
5. Archive OpenSpec; DogWalk can depend on this change

Rollback: revert ABI fields and leave Behaviours without Input (no data migration).

## Open Questions

None for this slice — grilled decisions are locked in ADR 0015 / CONTEXT.
