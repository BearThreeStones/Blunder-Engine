## 1. Docs and pipeline flip (glTF Intermediate)

- [x] 1.1 Align `CONTENT_LAYOUT.md` / import extension tables with ADR 0019 (glTF Intermediate; COLLADA removed)
- [x] 1.2 Remove or gate Assimp COLLADA Intermediate-direct Import and glTF→COLLADA upgrade on registry scan
- [x] 1.3 Restore/ensure Fast Path + Cook mesh load from Intermediate glTF/GLB (cgltf or chosen reader)
- [x] 1.4 Add GUID-preserving migration for remaining `.dae` Intermediate → glTF (or Reimport from Source); tests for no dae upgrade of glTF
- [x] 1.5 Update asset-pipeline unit/smoke tests that still assert COLLADA Intermediate

## 2. AnimationClip Asset + Import extract

- [x] 2.1 Define AnimationClip descriptor + readable YAML Intermediate schema (bones, times, TRS, Constant|Linear)
- [x] 2.2 On glTF Import: register 1 Mesh Asset (+ skin/bind) and N AnimationClip Assets with YAML extraction
- [x] 2.3 Reimport refreshes clip YAML while preserving stable clip GUIDs
- [x] 2.4 Dependency graph + Asset Watch: clip YAML / descriptor invalidate clip Finals; consumer→clip edges

## 3. Skeleton + AnimationPlayer runtime

- [x] 3.1 ClassDB Skeleton (rest/bind, bone poses) on Object
- [x] 3.2 ClassDB AnimationPlayer: name→GUID map, Play/Stop/Loop, hard cut, playback position/length
- [x] 3.3 Sampler: Constant + Linear onto co-located Skeleton; reject/ignore Phase 1 cross-Object drive
- [x] 3.4 Frame order: Behaviour Tick → sample → PoseApplied signal/callback
- [ ] 3.5 Generate Blunder.Api / C-ABI bindings for Play and PoseApplied subscription
- [ ] 3.6 Scene serialize/deserialize AnimationPlayer map + Skeleton presence on entities

## 4. Skinning paths

- [ ] 4.1 CPU skinning for Fast Path skinned meshes from Intermediate glTF weights + Skeleton pose
- [ ] 4.2 Cook skinned Final (GPU bone palette / skinning shader inputs)
- [ ] 4.3 GPU skinned draw for fresh Final; Editor Final and Player share path
- [ ] 4.4 Pose parity test: one frame CPU vs GPU (or CPU vs reference) within tolerance

## 5. Edit Mode preview

- [ ] 5.1 Authorship viewport controls to Play/Pause/Stop/Loop AnimationPlayer without DotNetHost
- [ ] 5.2 Verify Behaviour Tick does not run on Edit preview path

## 6. Content: test rig + Stepped feel

- [ ] 6.1 Author minimal skinned test-rig glTF (idle + walk) into Test/DogWalk Project; Import → clips + player map auto-fill
- [ ] 6.2 C# ValueSlicer utility + PoseApplied step sync; facing uses sliced visual, move stays real-time
- [ ] 6.3 Play acceptance: test-rig idle↔walk hard cut + deformation (engineering gate)
- [ ] 6.4 Import Chocomel (or agreed subset); Play acceptance: idle↔walk, real-time move, stepped facing (Done criteria)
- [ ] 6.5 Inspector: confirm name→GUID map editable after auto-fill

## 7. Validation

- [ ] 7.1 Automated tests: glTF Intermediate import, clip YAML extract, sampler Constant/Linear, no COLLADA upgrade
- [ ] 7.2 Manual checklist: Edit preview; Play Pause/Stop; Fast Path CPU then Cook GPU visual smoke
- [ ] 7.3 Update CONTEXT / agent docs only if apply drifts from grilled terms (prefer no churn)
