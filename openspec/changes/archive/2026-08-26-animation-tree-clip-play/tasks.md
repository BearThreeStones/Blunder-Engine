## 1. Runtime Clip Play

- [x] 1.1 Add Clip Play override state on `AnimationTree` (`clipPlay` / `clearClipPlay`, logical name + clock). Fail with no mutation on empty name, missing Clip Binding, or inactive tree
- [x] 1.2 Sample override as base in `sampleBaseOntoSkeleton` (OneShot still occupies first). Advance the Clip Play clock under TimeScale; hold last key past duration. Same-name / new-name Play hard-cuts to 0
- [x] 1.3 Skip `evaluateTransitions` while override is set. `travel` / `start` clear override then apply the state (including Travel to the remembered current state)
- [x] 1.4 Dominant / ruler / method-key clock: OneShot > Clip Play override > existing BlendSpace/clip base. Add2 still applies after the sampled pose. Fire does not clear override; Clip Play clock keeps advancing during Fire
- [x] 1.5 Do not serialize override (scene export, Tree Asset instance overrides, ClassDB properties)

## 2. Engine tests

- [x] 2.1 `animation_tree_test`: Play Hit on active tree with binding; pose is Hit; BlendSpace not sampled
- [x] 2.2 Same-name restart at 0; hold last key after duration; Travel/Start clear and resume remembered state
- [x] 2.3 Auto-transition true while override: state name unchanged, pose stays Hit
- [x] 2.4 Fire then return to Hit; Add2 still applies; inactive / missing / empty name fail with no mutation
- [x] 2.5 Round-trip scene (or export) with live override: loaded tree has no override (`scene_behaviour_instantiate_test` or tree export helper)

## 3. C-ABI and Blunder.Api

- [x] 3.1 `blunder_animation_tree_play`; bump `BLUNDER_ENGINE_C_ABI_VERSION` to 12; fill NativeAbi table (append pointer)
- [x] 3.2 `Blunder.Api` `AnimationTree.Play`; `Native.cs` / `NativeAbi.cs` layout match; NativeAbiTests stubs + completeness
- [x] 3.3 `animation_tree_c_abi_test` + `native_abi_test` + `engine_c_abi_test` version >= 12; managed NativeAbiTests compile

## 4. Animation Window

- [x] 4.1 Preview `stop` / rebind Stop call `clearClipPlay` with Fire clear; ruler uses Clip Play clip when override is set and Fire is empty
- [x] 4.2 `animation_preview_controller_test`: Stop and rebind clear override; End CINE does not; no new Window Clip Play chrome

## 5. Validation

- [x] 5.1 `openspec validate animation-tree-clip-play --strict --no-interactive`
- [x] 5.2 Run `animation_tree_test`, `animation_tree_c_abi_test`, `animation_preview_controller_test`, `native_abi_test`, `engine_c_abi_test`
