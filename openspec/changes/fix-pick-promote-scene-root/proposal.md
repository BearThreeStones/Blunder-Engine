## Why

Viewport mesh pick promotes GPU hits only to the **immediate hierarchy parent**. glTF-imported scenes insert intermediate transform nodes (`node` → `node_prim0`) between the scene-authored root (`BoxFront`, `BoxMid`, `BoxBack`) and the pickable leaf mesh. Users click a box in the viewport expecting the **scene root entity** to highlight in Hierarchy and drive gizmo/outline — but selection lands on an invisible `node` child instead. Hierarchy direct-click works; viewport pick appears broken.

Manual QA on `pick_test.scene.asset` fails for BoxFront/BoxMid selection, same-pixel cycling (#3), and parent promotion (#5) because peel lists dedupe to three identically named `node` entities rather than three distinct box roots.

## What Changes

- **Promotion walk-up:** `promotePickEntity` SHALL walk from the leaf hit upward while the current entity's parent has a valid grandparent, stopping at the **scene root child** (entity whose parent is invalid, or entity directly under a scene root when parent is the authored container). Single-level immediate-parent promotion is **replaced** for viewport pick, piercing menu, and click-cycle paths.
- **Sync peel list on first click:** After sync-then-async left-click, populate `m_last_peel_hits` from a synchronous multi-hit query (or promoted async peel) so the **second** click can cycle without waiting for full async `multi_peel` completion when ≥2 distinct promoted entities exist at the pixel.
- **pick_test camera framing:** Ensure editor startup frames the pick-test scene AABB so default view overlaps all three boxes along ±Y (or document required camera pose in QA).
- **Docs:** Update `render-pipeline.md` and supersede `hybrid-gpu-picking` design decision §2 (immediate parent only).

## Capabilities

### New Capabilities

<!-- None — behavior change to existing pick capabilities -->

### Modified Capabilities

- `viewport-mesh-pick`: Parent promotion walks to scene-authored root entity; sync peel list enables same-pixel cycling on second click; QA scenarios reference promoted scene roots.
- `viewport-piercing-menu`: Menu rows and cached cycle list use walk-up promoted entity ids and display names (`BoxFront`, not `node`).

## Impact

- `engine/src/runtime/function/render/pick/pick_entity_promotion.h` — walk-up promotion algorithm
- `engine/src/runtime/function/editor/viewport_pick_system.cpp` — sync peel list population on first click
- `engine/src/runtime/function/render/overlay/pick_overlay.cpp` / `hybrid_gpu_pick_system.cpp` — apply promotion consistently on peel results
- `openspec/changes/hybrid-gpu-picking/design.md` — cross-reference (decision superseded; archive note when both land)
- `docs/agents/render-pipeline.md` — promotion rule documentation
- Manual QA: `pick_test.scene.asset` BoxFront / BoxMid / BoxBack viewport pick, cycling, piercing menu row count
