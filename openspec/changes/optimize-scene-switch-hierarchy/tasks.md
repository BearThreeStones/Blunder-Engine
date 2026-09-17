## 1. Regression Evidence

- [x] 1.1 Add nested Hierarchy assertions that preserve row order, depth, expansion, last-sibling guides, icons, and tombstone filtering.
- [x] 1.2 Add the 10,000-root in-memory Hierarchy benchmark and confirm the current quadratic implementation fails the 500 ms Debug budget.

## 2. Linear Hierarchy Reconstruction

- [x] 2.1 Build a temporary parent-to-children index in one entity pass and traverse it without whole-scene child scans.
- [x] 2.2 Run the focused Hierarchy test and confirm both semantic cases and the performance budget pass.
- [x] 2.3 Add a 10,000-row bound-Object benchmark and confirm per-row attachment lookup still fails the 500 ms Debug budget.
- [x] 2.4 Maintain an EntityId-to-ObjectId lookup index across SceneInstance instantiate, ensure, release, clear, and export paths.
- [x] 2.5 Re-run focused lifecycle, Hierarchy semantics, and plain/bound-Object performance tests.
- [x] 2.6 Add a parent/child bound-Object release test and confirm recursive destruction invalidates the child binding.
- [x] 2.7 Preserve tracked descendant bindings when releasing a parent Object and validate indexed Object identity.
- [x] 2.8 Re-run the focused lifecycle and performance regression suite.

## 3. Validation

- [x] 3.1 Build `engine_editor` with the Windows Debug preset and run the relevant first-party test target.
- [x] 3.2 Exercise scene activation in `engine_editor`, capture before/after Hierarchy timing evidence, and verify rows, expansion, line guides, and icons remain correct.
- [x] 3.3 Run strict OpenSpec validation for `optimize-scene-switch-hierarchy`.
