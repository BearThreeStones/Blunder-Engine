## 1. Line-flag helper + unit test

- [x] 1.1 Add a header-only helper (`hierarchy_line.h` or equivalent) for last-sibling and `childAncestorContMask(parent_depth, parent_is_last, parent_mask)`
- [x] 1.2 Add `engine/src/tests/hierarchy_line_test.cpp` covering last-child stop vs non-last ancestor continuation
- [x] 1.3 Register the test in `engine/src/tests/CMakeLists.txt` (header-only include path, no `SceneInstance`); verify it passes

## 2. Flatten emits gutter flags

- [x] 2.1 Add `is_last_sibling` and `ancestor_cont_mask` to `EditorHierarchyTreeRow`
- [x] 2.2 Compute those fields in `HierarchySystem::appendVisibleSubtree` using the helper while walking each parent's visible children
- [x] 2.3 Add matching `is-last-sibling` and `ancestor-cont-mask` to `HierarchyTreeRow` in `hierarchy.slint`

## 3. Hierarchy Panel layout and Hierarchy Line

- [x] 3.1 Replace `depth * 12px` padding with per-depth gutter cells (`for column in row.depth`), indent step = chevron column width (18px)
- [x] 3.2 Draw T vs L on cell `depth-1` from `is-last-sibling`; draw through-stems on earlier cells from `ancestor-cont-mask`; line `#737373` / 1px
- [x] 3.3 Left-align row names (`horizontal-alignment: left`); keep an empty chevron slot on leaves
- [x] 3.4 Full-row `TouchArea` → `entity-selected`; chevron `TouchArea` only when `has-children` → `entity-toggle`

## 4. Docked and floating sync

- [x] 4.1 Copy the new fields in `SlintSystem::syncHierarchy`
- [x] 4.2 Add the same fields to `NativeFloatHierarchyRow` and map them in floating snapshot copy + `applySnapshotToEntry`

## 5. Validation

- [x] 5.1 Build `engine_editor` Debug
- [x] 5.2 Manual: nested last-child stem stops; non-last ancestor continues; leaf names align; roots have no incoming line
- [x] 5.3 Manual: gutter and empty chevron slot select; chevron toggles expand; Content Browser unchanged; floating Hierarchy Panel still draws lines
- [x] 5.4 `openspec validate hierarchy-line --strict`
