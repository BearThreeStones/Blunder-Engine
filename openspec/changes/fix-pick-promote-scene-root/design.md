## Context

`promotePickEntity` (introduced in `hybrid-gpu-picking` P0) promotes a GPU leaf hit by **one** `Entity::getParentId()` step. glTF instantiation adds a fixed depth-2 chain under each scene-authored root:

```
BoxFront (scene root, parent = invalid)
  └─ node (glTF transform, no MeshRenderer)
       └─ node_prim0 (leaf, MeshRenderer)
```

Viewport pick hits `node_prim0` → promotes to `node`. Hierarchy highlights `node`, not `BoxFront`. User confirmed Hierarchy direct-click on `BoxFront` works (selection → gizmo → outline pipeline is healthy).

`hybrid-gpu-picking` design §2 locked "immediate hierarchy parent only" per an earlier user answer ("父 mesh"). The pick_test QA and glTF nesting show that rule is insufficient: the meaningful selection target is the **scene-authored root entity** (direct child of the scene instance root list), not glTF plumbing nodes.

Same-pixel cycling fails partly because async `multi_peel` fills `m_last_peel_hits` one pass per frame; repeat clicks before completion see `size() <= 1`. Peel lists also dedupe to three `node` ids instead of `BoxFront` / `BoxMid` / `BoxBack`.

## Goals / Non-Goals

**Goals:**

- Viewport left-click, piercing menu rows, and click-cycle list promote leaf hits to the **scene-root child** entity users see in Hierarchy (`BoxFront`, etc.).
- Second left-click at the same pixel cycles through ≥2 promoted scene roots when boxes overlap (pick_test ±Y view).
- Keep sync-then-async first-click behavior (immediate selection + gizmo/outline).
- Unit-test promotion walk-up for glTF nesting and simple hierarchies.

**Non-Goals:**

- Promoting to arbitrary "logical groups" or named pick groups beyond scene-root-child rule.
- Changing gizmo/navigate-gizmo priority or async fence architecture.
- Flattening glTF import hierarchy in assets (runtime promotion only).
- P2 compute broad phase or GPU instance buffer (stays in `hybrid-gpu-picking`).

## Decisions

### 1. Scene-root-child promotion (replaces immediate-parent-only)

**Decision:** Replace `promotePickEntity` walk:

```
current = leaf
loop:
  parent = current.parent
  if parent invalid: return current
  grandparent = parent.parent
  if grandparent invalid: return parent   // parent is scene-root child (BoxFront)
  current = parent
```

Apply via existing `promotePickEntity`, `promotePickEntityList`, and `dedupePromotedPickHits` — all call sites pick up the new semantics without API change.

**Rationale:** Matches editor Hierarchy top-level rows for glTF-imported props. One function, one rule, all pick surfaces consistent.

**Alternatives considered:**

- **Immediate parent only (status quo)** — fails pick_test; user-reported QA failure.
- **Walk to absolute root ancestor** — over-promotes authored intermediate groups (`SceneRoot > PickGroup > Mesh` would select `SceneRoot` instead of `PickGroup`). Deferred; add explicit `PickGroup` component later if needed.
- **Promote only when parent has no MeshRenderer** — heuristic, harder to test, breaks if transform node gains mesh.

### 2. Sync-fill peel list on first left-click

**Decision:** In `onViewportLeftReleased`, after synchronous `pickEntityAtWindowPosition` + promotion + `applySelection`, call synchronous `pickAllEntitiesAtWindowPosition` (entity-ID peel, existing `PickOverlay` path), run `dedupePromotedPickHits`, assign to `m_last_peel_hits` before `requestPick(..., multi_peel)`.

Async `multi_peel` still runs to refresh the list when complete; `pickWithCyclingFromResult` already prefers cached `m_last_peel_hits` when async returns ≤1 promoted hit on repeat click.

**Rationale:** Enables second-click cycling without waiting N frames for async peel. Minimal change; reuses proven sync peel path already used elsewhere.

**Alternatives considered:**

- **Async-only peel list** — preserves non-blocking input but fails fast repeat-click QA.
- **Block until multi_peel completes** — violates hybrid P0 latency goals.

### 3. pick_test camera framing

**Decision:** Rely on existing `m_pending_scene_camera_focus` / `snapFocusOnAABB` when `pick_test` loads (already in `RenderSystem::tickVulkan` if scene has world bounds). Verify `pick_test.scene.asset` produces valid `hasWorldBounds()` after mesh cook; add explicit startup focus flag if default camera still misses overlap pixel.

**Rationale:** Avoids hard-coded camera; uses scene AABB.

### 4. Supersede hybrid-gpu-picking design §2

**Decision:** This change **MODIFIED** the promotion requirement in `viewport-mesh-pick`. When archiving `hybrid-gpu-picking`, note design §2 is superseded by `fix-pick-promote-scene-root`. Do not edit archived hybrid change in place.

## Risks / Trade-offs

- **[Risk] Intermediate authored groups skipped** — `SceneRoot > Group > Mesh` promotes to `SceneRoot`, not `Group`. → Document; future `PickGroup` tag if needed.
- **[Risk] Sync peel on first click adds GPU stall** — Same class as existing sync-then-async first hit; bounded by `k_max_peel_count`. → Acceptable for P0; removed when P2 broad list replaces peel.
- **[Risk] Deep glTF chains (>2 intermediates)** — Walk-up continues until scene-root child; deeper chains still resolve correctly.
- **[Risk] Peel order vs promotion** — Entity-ID peel excludes leaf ids; promotion after each peel pass must use new walk-up (already centralized in `dedupePromotedPickHits`).

## Migration Plan

1. Implement `promotePickEntity` walk-up + unit tests.
2. Sync-fill `m_last_peel_hits` in `onViewportLeftReleased`.
3. Manual QA on `pick_test`: viewport click BoxFront, second-click cycle BoxMid/BoxBack, Ctrl+right menu 3 rows named `BoxFront`/`BoxMid`/`BoxBack`.
4. Update `docs/agents/render-pipeline.md` promotion section.
5. Archive this change; reconcile `hybrid-gpu-picking` tasks 1.4 / design §2 on hybrid archive.

## Open Questions

- Should piercing menu labels show only promoted name, or `leafName (BoxFront)` for deep debugging? (Default: promoted name only.)
- Add dedicated `pick_test_parent.scene` with `PickGroup > Child` for intermediate-group regression later? (Out of scope unless QA needs it.)
