## 1. Fog Component (CPU / document)

- [x] 1.1 Add `FogComponent` on `SceneInstance` (enabled, volumetric enabled, density, height falloff, view distance, albedo, `g`) plus `"fog"` JSON round-trip and verify `scene_serializer_test` (or a dedicated fog serializer test) covers present and absent keys
- [x] 1.2 Pick the first active Fog in stable EntityId order (Fog enabled, volumetric on, Active in Hierarchy, view distance > 0) and verify a unit test that the second Fog is ignored
- [x] 1.3 Add… Fog Unique (`InspectorUniqueKind::Fog`), no Object materialization, present-only Inspector section with defaults (density 0.02, falloff 0.2, 60 m, white, `g` 0.2) and verify undo Add Fog / redo via Document History
- [x] 1.4 Fog Add… kind icon + Hierarchy row icon when present and verify the Fog row is disabled in Add… when already attached
- [x] 1.5 Add Fog Component / volumetric fog glossary to `CONTEXT.md` (Unique lists include Fog) and verify the terms match `scene-fog-component` defaults

## 2. Volume math (CPU, can land before GPU)

- [x] 2.1 Implement froxel XY = ceil(view extent / 16), Z = 64, exponential slice map with S = 32, near offset 9.5 cm, far = Fog view distance, and verify a unit test against known (z ↔ slice) pairs
- [x] 2.2 Implement Z-up height density `ρ · exp2(−falloff · (world.z − fogHeight))` and verify a unit test that world Y does not change density when Z is held fixed
- [x] 2.3 Host gate `shouldApplyVolumetricFog(Editor|Player, activeFog)` vs no Fog and verify unit tests for Player-on, Viewport-on, missing-Fog-off

## 3. Player volume GPU (Windows worker when the dirty tree is free)

Do not start this group until mesh-shader-shadows / other GPU dirt is clear. Do not amend those changes.

- [x] 3.1 Add `VolumetricFogPass` (3D RGBA16F density, scatter ping/pong, integrated) sized from the Player offscreen, hand-written layouts, not on the Bindless color table, and verify process start still matches shader-resource-layout exact-binding rules for mesh pipelines
- [x] 3.2 Density CS writes scattering/extinction from the active Fog at each froxel world position (Z-up) and verify a GPU/debug dump or Headless still of a known-height fixture is denser near the Fog height than above it
- [x] 3.3 Scatter CS injects one unshadowed illuminating Directional × HG(`g`) with shadow factor 1, skips point/spot, and verify a scene with only points stays extinction-only while a directional adds in-scatter
- [ ] 3.4 Temporal: Halton XY jitter, mix 20% current / 80% history on scatter, skip out-of-frustum history and `max()` keep, then integrate CS (8×8×1 loop Z) storing in-scatter + T, and verify a still camera does not show raw 64-slice banding as badly as temporal-off
- [x] 3.5 Fullscreen composite after Viewport lighting / Forward on Editor and Player: `color * T + inscatter`, including far-slice sky; Deferred Viewport composites on the presented color with no new GBuffer. CPU/GPU wired; visual walk remains 4.3
- [x] 3.6 Wire CMake (`engine/src/runtime/CMakeLists.txt`, Slang under `engine/shaders/`) and verify `engine_player` links the pass on the documented Windows preset

## 4. Validation

- [x] 4.1 Run first-party tests added in groups 1–2 (`ctest` or the test executable, not compile-only) and verify they pass
- [x] 4.2 `openspec validate volumetric-fog --strict` and verify it succeeds
- [ ] 4.3 Manual Player smoke per `manual-checklist.md` (Windows, after GPU tasks) — Status remains Not run until the human walks the stories
