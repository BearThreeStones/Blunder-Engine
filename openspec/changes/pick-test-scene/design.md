## Context

The default editor scene (`root.scene.asset`) loads Sponza plus a nested `sub.scene.asset`, which is unsuitable for validating hybrid GPU picking (multi-hit peel, piercing menu, click cycling). The engine has no runtime "delete entity" API; test geometry must be authored as assets on disk.

`hybrid-gpu-picking` P0 is in progress; manual tasks 1.6 and 4.2–4.4 need a controlled fixture. This change adds content only—no pick-algorithm changes.

Coordinate system: engine Z-up (+X right, +Y forward, +Z up). Scene `.scene.asset` positions are in engine space.

## Goals / Non-Goals

**Goals:**

- Ship a **1×1×1 m** (approx) cube glTF + `.mesh.yaml` under project `Resources/` / `Assets/`.
- Ship `pick_test.scene.asset` with **three root boxes** stacked along **+Y** so default camera sees depth overlap.
- Simplify `root.scene.asset` (remove Sponza / SubLevel).
- Default editor startup → `pick_test.scene.asset`.
- Document expected QA signals in `docs/agents/`.

**Non-Goals:**

- Editor UI for spawning/deleting primitives.
- A separate parent/child hierarchy fixture in this change (can be a follow-up `pick_test_hierarchy.scene.asset`).
- Changing pick promotion or peel logic (`hybrid-gpu-picking`).
- Removing Sponza from `Resources/` (only from default scene references).

## Decisions

### 1. Cube source: vendored glTF 2.0 from repo third-party

**Decision:** Copy `engine/3rdparty/ktx/tests/webgl/libktx-gltf/assets/Cube.gltf` and its binary buffer into `Resources/Models/PickTest/`, rather than authoring glTF by hand or adding assimp `TwoBoxes` (glTF 1.0).

**Rationale:** glTF 2.0, already validated by cgltf import path; single primitive; small footprint.

**Alternatives considered:**

- Procedural mesh in code — rejected (scope creep).
- Reuse Sponza scaled — rejected (still complex hierarchy).

### 2. Three root entities, same mesh descriptor

**Decision:** One `Cube.mesh.yaml`; three scene entities each with `"mesh": "assets/Meshes/Cube.mesh.yaml"`. `SceneSystem::attachSceneEntityMeshes` imports under each entity independently.

**Rationale:** Matches existing scene format; yields three distinct `EntityId` roots for pick lists.

**Positions (engine space, tentative):**

| Entity   | Position `[x, y, z]` | Role        |
|----------|----------------------|-------------|
| BoxFront | `[0, 2.0, 1.0]`      | Nearest     |
| BoxMid   | `[0, 3.2, 1.0]`      | Middle      |
| BoxBack  | `[0, 4.4, 1.0]`      | Farthest    |

Adjust after in-editor camera framing so viewport-center ray hits all three. Z=1 lifts boxes above ground plane for grid visibility.

### 3. Startup scene switch

**Decision:** Change `k_startup_scene_path` in `engine.cpp` to `assets/Scenes/pick_test.scene.asset`. Add optional `BLUNDER_STARTUP_SCENE` env override for developers who need Sponza.

**Rationale:** Zero UI steps for QA; reversible via env without rebuild if override is implemented.

**Alternatives considered:**

- Keep Sponza startup, document manual open — rejected (user asked for simple default).

### 4. root.scene.asset cleanup

**Decision:** Remove `Sponza.mesh` entity and `childScenes` block; retain `Sun` / `CameraRig` for any code paths still opening root.

**Rationale:** Prevents accidental heavy load; Sponza remains available via Content Browser spawn.

### 5. GUID and cook

**Decision:** Generate a new UUID for `Cube.mesh.yaml`; run `asset_compiler` or rely on editor `cookIfStale` at startup. Do not hand-edit `.blunder/asset_registry.yaml`.

## Risks / Trade-offs

- **[Risk] Default scene change surprises demo users** → Mitigation: `BLUNDER_STARTUP_SCENE=assets/Scenes/root.scene.asset` escape hatch; note in `docs/agents/build.md`.
- **[Risk] Box positions don't overlap at default camera** → Mitigation: tune positions in task 3; verify with focus-on-scene after load.
- **[Risk] Cube glTF missing external `.bin`** → Mitigation: verify copy includes buffer file or embedded URI during apply.

## Migration Plan

1. Add Resources + mesh YAML + pick_test scene.
2. Trim root.scene.asset.
3. Switch startup path + env override.
4. Cook assets; launch editor; verify hierarchy shows 3 boxes and Ctrl+right menu lists 3 rows (after `hybrid-gpu-picking` peel fix if needed).
5. Rollback: revert startup constant and restore root.scene entities.

## Open Questions

- Should we add a second scene `pick_test_parent.scene.asset` (parent + two child meshes) for parent-promotion dedupe QA in the same change?
- Exact box positions may need one in-editor iteration after first load.
