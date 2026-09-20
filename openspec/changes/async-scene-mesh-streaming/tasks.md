# Tasks

## 1. Mesh Loader CPU

- [ ] 1.1 Add Mesh Loader sources under `engine/src/runtime/function/render/` (request by Mesh Asset GUID, in-flight map, generation id) and verify a `mesh_loader_test` (or equivalent) constructs the Loader without Vulkan
- [ ] 1.2 Submit unique-mesh CPU read (cooked `.meshbin` / Fast Path into Job data) as Jobs in Scene Asset document order, coalesce duplicate GUIDs, and verify the test queues GUID A before GUID B and starts at most one Job per GUID
- [ ] 1.3 Poll Job done/failed on the tick without `JobSystem::wait()` to present, and verify `loadScene` on a multi-GUID fixture returns before all unique-mesh Jobs finish

## 2. GpuMesh budget and skip-draw

- [ ] 2.1 Create `GpuMesh` on the RHI tick with a unique-mesh (or byte) budget, never inside a Job, and verify one scene-to-render sync on the fixture does not `GpuMesh::create` every unique mesh
- [ ] 2.2 Bind MeshRenderer when that GUID is CPU-resident; skip draw until `GpuMesh` exists (no placeholder box), and verify `syncSceneToRender` draws only resident unique meshes
- [ ] 2.3 Failed unique GUID skips that class only, and verify a fixture with one missing GUID plus one valid neighbor keeps the valid renderers

## 3. Hosts, cancel, Headless

- [ ] 3.1 Wire Mesh Loader onto RenderSystem for Editor and Player; `openScene` returns after entity instantiate + enqueue, and verify editor open of the fixture does not wait unique GPU residency
- [ ] 3.2 `engine_player` reaches Iterate without waiting all unique Mesh CPU+GPU, streams in-process, and verify Player load of the fixture returns before unique Jobs complete (no Live editor pointers)
- [ ] 3.3 Scene drop bumps generation; stale completions do not bind or keep dropped `GpuMesh`, and verify a switch-scene test drops the old GUID without crash
- [ ] 3.4 Headless / no device skips `GpuMesh` upload (CPU Jobs MAY run), and verify Headless start/exit on the fixture does not create `GpuMesh`

## 4. DogWalk contract (apply later; not this planning commit)

- [ ] 4.1 Keep GUID Mesh bind as the streaming unit (no per-instance `openGltfImportDocument` stream) and verify the synthetic fixture uses Mesh Asset GUIDs, not one glTF open per renderer
- [ ] 4.2 Do not add loading-bar UI, binary Scene Asset, frustum Job order, or a first-frame ms gate, and verify no new Slint mesh-loading chrome and Job order remains document order
- [ ] 4.3 Human walk remains DogWalk `assets/Scenes/se-world.scene.asset` with GUID bind (not Test/Sponza, not collision QC `root.scene.asset`, not the ~10k glTF collision exe); checklist stays **Not run** until the human walks it

## 5. Docs and validate

- [ ] 5.1 Keep `CONTEXT.md` Mesh Loader terms aligned with implementation names; ADR 0072 stays the unique-mesh vs Texture Loader record
- [ ] 5.2 `openspec validate async-scene-mesh-streaming --strict` passes
- [ ] 5.3 Do not edit `cursor/scene-collision-bridge-8a89` or PR 24; do not commit on flatten PR 31; verify this change’s diff has no files from those branches
