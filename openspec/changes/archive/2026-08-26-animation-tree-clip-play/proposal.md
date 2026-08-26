## Why

Scripts need Animancer-like “play this clip now” without a Unity PlayableGraph or a second playback Unique. Today they can only `Travel` / `Start` authored states or `RequestOneShot` (returns to base). Grilling locked **Clip Play** on **AnimationTree** ([ADR 0045](../../../docs/adr/0045-code-first-play-on-animation-tree.md); [CONTEXT.md — Clip Play](../../../CONTEXT.md)). Ship the v1 runtime surface now: hard-cut base replace, tests, C-ABI / Blunder.Api.

## What Changes

- Add **Clip Play** on the co-located **AnimationTree**: script names a **Clip Binding** logical name and **replaces the tree base** with that one clip (hard cut, clock 0).
- **Clip Play override** is session-only: not serialized, not an AnimationTree Asset instance override, not Document History.
- Override does not auto-return; last key **holds** until **Travel** / **Start**. Authored StateMachine transitions do not auto-Travel while set. Current state name is remembered and not sampled.
- Fire slot and Add2 keep current stacking; Fire does not clear the override; Clip Play clock keeps advancing during Fire.
- Fail with no mutation: empty name, incomplete Clip Binding, inactive tree (no auto-activate). Same-name Play hard-cuts from 0.
- C-ABI + NativeAbi table + `Blunder.Api` method (C# may spell `Play`; product term is Clip Play). ABI version bump.
- Animation Window: no Clip Play control; **Stop** / rebind Stop **clears** the override (like Fire); ruler follows the Clip Play clip when override is set and Fire is not occupying.

**Out of scope (this change):** fade / Crossfade / snapshot mixer; unified BlendSpec; Animancer Unique; PlayableGraph; Animation Window Clip Play chrome; Canvas topology; DogWalk Behaviour content Done; AnimationClip loop field; queuing Clip Plays.

## Capabilities

### New Capabilities
- `animation-tree-clip-play`: Clip Play / Clip Play override — addressing, hard-cut base replace, hold last key, transition suspend, Fire/Add2 stack, failure contract, non-persistence, C-ABI / Blunder.Api

### Modified Capabilities
- `animation-window`: Stop and rebind Stop clear Clip Play override; ruler uses Clip Play clip when override is set and Fire/OneShot is not occupying; no v1 Clip Play chrome
- `animation-pipeline`: Tree blend specification MAY take Clip Play override as the base (still exactly one spec per evaluate; not unified BlendSpec)
- `engine-c-abi`: Clip Play entry point; ABI version >= 12
- `script-native-abi`: NativeAbi completeness includes the Clip Play pointer

## Impact

- Engine: `AnimationTree` sample/advance — override flag, skip `evaluateTransitions` while set, sample Clip Play as base, Travel/Start/Stop-preview clear override
- `engine_c_abi` / `BlunderNativeAbi` / `Blunder.Api.AnimationTree` / NativeAbiTests
- Animation Window preview controller Stop/rebind
- Tests in `animation_tree_test` (and window Stop if a hook exists)
- Docs: CONTEXT Clip Play terms and ADR 0045 already locked
- Non-impact: AnimationPlayer.Play product path stays retired; Canvas; per-edge fade
