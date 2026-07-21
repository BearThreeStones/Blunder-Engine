## Why

Play Mode can host C# Behaviours and Tick them in the Player, but Behaviours still have no product **Gameplay Input** — no way to read Move/Jump without raw keys or the editor’s legacy `GameCommand` bitfield. DogWalk and every movement Behaviour are blocked until Actions reach `Blunder.Api` through the existing NativeAbi path.

## What Changes

- Add a Player-authoritative **Gameplay Input** sampler that publishes named **Gameplay Actions** each simulation frame (starter set: 2D **Move** + **Jump** press-edge)
- Expose Actions to Behaviours via a static `Blunder.Api` **`Input`** façade (not Object/ClassDB properties, not raw KeyCode as the primary API)
- Extend the existing **`BlunderNativeAbi`** table with gameplay-input entry points (**BREAKING** layout / ABI version bump for ScriptHost registration)
- Keep editor `InputSystem` / `GameCommand` as a separate camera/editor path; do not treat Edit Mode or the editor window as the product input source
- Enforce Pause / focus rules: Pause discards Action edges (no buffered Jump on Resume); unfocused Player → idle Actions

**Out of scope:** remappable bindings, gamepad, raw KeyCode primary API, separate Input-only ABI registration, Pause edge buffering, DogWalk content itself

## Capabilities

### New Capabilities
- `gameplay-input`: Player-side Action sampling, Pause/focus idle rules, C-ABI + managed `Input` façade for Move (2D axis) and Jump (shared press-edge per sim frame)

### Modified Capabilities
- `engine-c-abi`: Add gameplay-input C entry points; bump ABI version for the extended NativeAbi table
- `script-native-abi`: Registered NativeAbi table MUST include the new gameplay-input pointers; incompleteness rejects registration

## Impact

- Native: new gameplay-input module + C-ABI symbols; `blunder_native_abi_fill_*`; Player tick sampling before Behaviour Tick; ABI version constant
- Managed: `Blunder.Api` `Input` (+ small Vec2 if needed), `NativeAbi`/`Native` layout and completeness checks, `Blunder.Api.NativeAbiTests`
- Tests: pure native Action math / Pause / focus / edge-sharing; managed façade stubs; optional Player host smoke
- Docs already grilled: `CONTEXT.md` (Gameplay Input / Gameplay Action), `docs/adr/0015-gameplay-input-nativeabi-actions.md`
- Implementation plan: `docs/superpowers/plans/2026-07-21-gameplay-input.md`
