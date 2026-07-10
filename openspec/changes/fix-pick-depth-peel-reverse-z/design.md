## Context

- Editor camera: `glm::perspectiveZO` + Y-flip (`editor_camera.cpp`) — Vulkan [0,1] depth, **near plane → 1.0**, **far → 0.0** (nearer fragments have **larger** `gl_FragCoord.z` / depth attachment values).
- Pick prepass: `pick_prepass.slang` peel branch discards when `sv_position.z <= peelDepth + peelEpsilon` — correct for classic OpenGL near=0, **wrong for ZO**.
- Pipeline: `VK_COMPARE_OP_LESS`, depth clear `1.0f` — combined with ZO this may prefer **farther** surfaces on multi-draw pixels; peel and front-most readback must be audited together.
- Async path: `HybridGpuPickSystem::poll` chains peel passes (1/frame) for `piercing_menu` and `multi_peel`; stops when `appendLeafHit` fails.
- QA scene: `pick_test.scene.asset` — three `Box*` roots; glTF import adds `node → node → node_prim0` under each; user confirmed menu/cycle show only one `node`.

## Goals / Non-Goals

**Goals:**

- Iterative GPU peel returns **N ≥ 2** distinct leaf hits when N surfaces overlap along the ray at a pixel.
- Piercing menu and same-pixel click cycling work on corrected `pick_test` layout.
- Single-hit front-most pick unchanged for non-overlapping pixels.
- Document depth convention for future P2 broad-phase work.

**Non-Goals:**

- Parent promotion policy (still one level to immediate parent).
- Compute broad-phase / removing peel (`hybrid-gpu-picking` P2).
- Changing glTF import hierarchy or piercing menu UI.

## Decisions

### 1. Fix peel discard for ZO depth

**Decision:** In `pick_prepass.slang`, peel pass discards fragments **at or in front of** the already-resolved surface:

```slang
// ZO: nearer = larger depth
if (ubo.isPeelPass != 0u && input.sv_position.z >= ubo.peelDepth - ubo.peelEpsilon)
    discard;
```

Pass `peelDepth` from readback depth buffer (first pass `isPeelPass = 0`).

**Alternatives considered:**

- Reproject linear eye-space depth in shader — more accurate but heavier; defer unless readback mismatch persists.
- Switch camera to non-reversed projection — large blast radius across render pipeline.

### 2. Align depth test with ZO front-most

**Decision:** Audit pick prepass depth state. If LESS + clear 1.0 inverts front-most on multi-draw pixels, switch pick-only pipeline to:

- `depthCompareOp = VK_COMPARE_OP_GREATER`
- clear depth to `0.0f`

Main viewport may already use GREATER elsewhere — match pick pass to the same convention.

**Rationale:** Peel readback must match what depth test considers “front-most”; otherwise pass-0 depth is wrong before peel even starts.

### 3. Keep async peel orchestration unchanged

**Decision:** No changes to `HybridGpuPickSystem` peel loop structure unless epsilon/sign needs tuning after shader fix.

**Rationale:** Bug is in depth semantics, not async scheduling.

### 4. Fix `pick_test` overlap for QA

**Decision:** Place `BoxFront` / `BoxMid` / `BoxBack` at **shared center** `[0, 3.2, 1.0]` (or spacing ≤ cube extent) so a camera focused on the scene sees three coincident shells along ±Y view rays. Update `docs/agents/render-pipeline.md` table accordingly.

**Alternatives:** Thin card proxies — simpler visually but different from mesh pick; shared center is enough for peel QA.

### 5. Verification harness

**Decision:** Add a dev-only or test log path: call sync `pickAllAtWindowPosition` at viewport center after scene load and assert `hits.size() >= 3` on `pick_test` with default camera focus. Optional `LOG_INFO` behind verbose flag if no test target exists.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Depth compare change breaks single pick | Manual + sync `pickAtWindowPosition` before/after on non-overlapping mesh |
| `sv_position.z` vs depth attachment mismatch | Compare readback depth to attachment at same pixel; adjust peel to use same space |
| Menu still shows `node` not `Box*` | Expected until promotion follow-up; QA counts **rows ≥ 3** (may be three `node` names) |
| Peel 1/frame feels slow for 16 hits | Acceptable for P0; P2 removes peel |

## Migration Plan

1. Land shader + pipeline depth fix.
2. Update `pick_test` scene positions.
3. Rebuild `engine_editor`; run manual QA checklist (piercing menu ≥3 rows, double-click cycles).
4. No data migration; no env flags.

## Open Questions

- Does main render pass use GREATER + clear 0 today? Pick pass should mirror it exactly.
- Should first left-click use `multi_peel` immediately when broad-phase is absent? (Defer — second-click multi-peel is existing spec.)
