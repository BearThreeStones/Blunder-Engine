## Context

See proposal.md for motivation. `HierarchyPanel` already flattens a visible entity tree (`depth`, `has-children`, `expanded`) and indents with `padding-left: row.depth * 12px`, but row `Text` stretches without `horizontal-alignment: left` (names read as centered) and no gutter geometry exists for Hierarchy Line.

`HierarchySystem::appendVisibleSubtree` walks `SceneInstance` children in order. Docked sync (`SlintSystem::syncHierarchy`) and floating snapshots (`NativeFloatHierarchyRow`) copy `HierarchyTreeRow` field-for-field today.

Grilling locked product grammar in `CONTEXT.md` (Hierarchy Panel / Hierarchy Line). This design only chooses how to emit and draw it.

## Goals / Non-Goals

**Goals:**

- Emit enough per-row gutter flags from the existing flatten pass to draw last-child L vs non-last T and ancestor continuation.
- Draw Hierarchy Line in Slint from those flags (docked and floating panels share `hierarchy.slint`).
- Make the whole row selectable, with expand-chevron toggle only when the row has children.
- Cover flag math with a small unit test that does not need `SceneInstance`.

**Non-Goals:**

- Sharing this gutter with Content Browser (proposal out of scope).
- A canvas/overlay pass outside the row repeater.
- Reparent, scene-title-as-root, or new hover.

## Decisions

### 1. Flatten emits `is_last_sibling` + `ancestor_cont_mask`

**Decision:** Extend `EditorHierarchyTreeRow` / `HierarchyTreeRow` with `is_last_sibling` (bool) and `ancestor_cont_mask` (int, bit `k` = draw a through-stem at indent column `k`). Compute them in `appendVisibleSubtree` while iterating each parent's visible children: the last child gets `is_last_sibling`; each child's mask is the parent's mask, plus bit `(parent.depth - 1)` when `parent.depth >= 1` and the parent is not last among its siblings. Depth-0 rows keep mask `0` and no incoming stem.

**Rationale:** The visible list is already flat; Slint cannot walk Scene Tree. Last-child stop and non-last continuation are properties of sibling order at flatten time. A 32-bit mask covers authored depth for this slice.

**Alternative:** Overlay Path drawn from window coordinates after layout — rejected; duplicates hit-testing and fights `Flickable` clip/scroll. **Alternative:** nested per-row model of gutter cell enums — rejected; extra VectorModel per row for no extra power.

### 2. Slint gutter cells via `for i in 0..row.depth`

**Decision:** Each row is `[gutter cells for 0 .. depth) | chevron column | name]`. Indent step equals chevron column width (18px) so column `d` of a child sits under the parent's chevron. Slint in this tree does not accept `0..n` range-for; `for column in row.depth` repeats `depth` times with `column` = 0..depth-1. Cell `i == depth-1` draws T (full-height stem + tick) or L (stem to mid + tick) from `is_last_sibling`. Cells `i < depth-1` draw a full-height 1px stem iff `ancestor_cont_mask & (1 << i) != 0`. Line color `#737373`. Names: `horizontal-alignment: left`; do not stretch-center `Text`.

**Rationale:** Integer-as-model repeater is what this Slint compiler accepts; one cell per ancestor depth keeps L/T geometry local to the row (scrolls and clips with `Flickable`). Matching indent to chevron width is what makes the stem read as “under the parent arrow” instead of a 12px pad that misses the arrow.

**Alternative:** Keep `depth * 12px` padding and overlay lines — rejected; arrows and stems would not share a column.

### 3. Hit testing: full-row select, chevron TouchArea only when `has-children`

**Decision:** Wrap the row in a `TouchArea` that fires `entity-selected`. Keep a child `TouchArea` on the chevron **only if** `has-children` (toggle). Empty chevron slot and gutter have no extra absorber, so they select. Chevron toggle may also select the same entity if both areas fire; that is acceptable.

**Rationale:** Matches the grilled contract (gutter + empty slot select; chevron toggles). Avoids today's dead zone on indent padding. No reparent.

**Alternative:** One TouchArea that hit-tests x against chevron bounds in C++ — rejected; Slint already owns the two widgets.

### 4. Copy new fields through floating snapshot

**Decision:** Add the same two fields to `NativeFloatHierarchyRow` and map them in `syncHierarchy` / floating snapshot copy. No separate floating layout.

**Rationale:** Floating Hierarchy Panel instantiates the same `HierarchyPanel`; missing fields would drop stems when undocked.

### 5. Unit-test mask update without SceneInstance

**Decision:** Extract a tiny pure helper (parent depth + parent-is-last + parent mask → child mask, plus last-sibling from child index/count) and test a nested last-child vs non-last-child table. Do not stand up `SceneInstance` for this slice. Visual grammar remains manual QA in `engine_editor`.

**Rationale:** `docs/agents/testing.md` prefers tests that skip `SceneInstance`. The bug surface is the bit math, not entity storage.

## Risks / Trade-offs

- **[Risk] Repeating gutter cells per row on huge trees is extra rectangles** → Mitigation: same visible row count as today; cells are 1px rectangles. Revisit only if a large scene measures a hit.
- **[Risk] Depth > 32 drops high ancestor bits** → Mitigation: clamp/saturate remaining through-stems; authored scenes this deep are not a product target. Do not fail flatten.
- **[Risk] Chevron-only TouchArea vs full-row select double-fires** → Mitigation: both target the same entity; toggle still wins as the extra action. Do not add reparent.
- **[Risk] Floating panel field drift** → Mitigation: tasks include snapshot mapping next to `syncHierarchy`.

## Migration Plan

Single PR; no scene-asset migration. Rollback is revert. Manual QA: nested last-child, non-last continuation, leaf alignment, gutter click, floating Hierarchy Panel, Content Browser unchanged.

## Open Questions

None.
