## Context

See `proposal.md` for motivation and `specs/hierarchy-panel/spec.md` for the performance contract.

`HierarchySystem::rebuildVisibleTree` currently obtains the children of each row through `SceneInstance::forEachChild`. That API scans the dense entity array, so rebuilding a visible tree of N flat entities performs roughly N² entity comparisons. Row icon generation also calls `SceneInstance::findBoundObject` once per visible row, and that lookup scans every bound Object. Scene activation runs both costs synchronously before the editor can present the new scene.

The hierarchy also owns expansion state, row ordering, tombstone filtering, line-guide metadata, and row icons. Those semantics must remain unchanged.

## Goals / Non-Goals

**Goals:**

- Reduce one Hierarchy reconstruction from quadratic to O(entity count + visible row count + rendered icon count).
- Preserve entity storage order for roots and siblings.
- Preserve expansion defaults, collapsed branches, tombstone filtering, line-guide metadata, and icon generation.
- Cover both semantics and the 10,000-row Debug performance budget for plain and bound-Object-dense scenes.

**Non-Goals:**

- Async scene deserialization, glTF parsing, CPU/GPU resource upload, or loading UI.
- A persistent SceneInstance child container or changes to EntityId storage.
- Reducing the number of existing scene-panel refresh calls.
- Changes to Slint models or the Slint fork.

## Decisions

### Build an ephemeral parent-to-children index per reconstruction

One dense entity pass will collect visible root IDs and append each non-root ID to a parent-keyed child vector. Tree traversal will read those vectors instead of rescanning every entity.

This keeps the index local to the rebuild, so soft delete, restore, reparenting, and scene replacement cannot leave persistent hierarchy state stale. A persistent SceneInstance child index was considered, but it would require every parent mutation path to maintain a new invariant and would broaden this performance fix.

### Preserve ordering by appending during the dense entity pass

Roots and each sibling vector will be populated in SceneInstance iteration order. No sorting is added, which preserves current row order and keeps construction linear.

### Keep row construction and icon generation unchanged

The traversal will continue to compute the same depth, expansion, last-sibling, ancestor continuation mask, and icon slots. Only child discovery and bound-Object lookup data structures change. This limits behavioral risk and keeps the optimization independent of attachment and rendering systems.

### Index bound Objects by EntityId

`SceneInstance` will retain its ordered ObjectId vector for ownership and destruction, while also maintaining an EntityId-to-ObjectId map for lookup. Scene instantiation and lazy binding add both entries; release and clear remove both entries. Export and Hierarchy icon generation can then reuse `findBoundObject` without an object-count scan.

Replacing the ownership vector entirely was considered, but its deterministic lifetime iteration is already used by clear and export. Keeping it avoids changing ownership semantics while the map acts only as a lookup index.

Before releasing one bound Object, SceneInstance will detach the nearest descendant Objects that are also tracked bindings. `ObjectDB::destroy` may then recursively remove untracked implementation children without invalidating other entity bindings or leaving stale lookup entries.

### Validate with semantic and bounded-time regression cases

A nested-tree case will assert row order and guide metadata. Two 10,000-root in-memory cases, one plain and one with a bound Object on every entity, will assert all rows are produced within the 500 ms Windows Debug budget. The bounded-time cases directly exercise both former quadratic paths without filesystem, asset, Vulkan, or Slint noise.

## Risks / Trade-offs

- [Wall-clock performance tests can vary on overloaded machines] → Use a budget far above expected linear runtime but below the observed quadratic runtime, and isolate the benchmark from IO and GPU work.
- [Temporary child vectors add O(N) memory during rebuild] → Release them when reconstruction returns; this replaces repeated scans and is bounded by entity count.
- [The bound-Object index can drift from the ownership vector] → Update both only at the two binding sites and clear/release sites, then retain lifecycle round-trip tests.
- [Malformed parent links remain hidden] → Preserve current behavior; parent validation and repair are outside this change.
- [Very deep trees still use recursive traversal] → Recursion depth is unchanged from current behavior; iterative traversal is a separate hardening concern.

## Migration Plan

No data migration or public API migration is required. The change can be rolled back by restoring the previous Hierarchy traversal; scene assets and runtime entity storage are unaffected.
