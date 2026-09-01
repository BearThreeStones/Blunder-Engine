# Manual checklist — animation-window-clip-anatomy

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Passed 2026-09-01 (human walked windowed `engine_editor`)

Windowed editor. Bind Chocomel (AnimationTree) unless a row says otherwise.

| # | User story | Pass |
|---|------------|------|
| 1 | Select Chocomel. Animation Window shows Clip anatomy for the current ruler clip: bone groups, only existing Position / Rotation / Scale rows (Editor Icon + word), diamonds at key times, playhead through ruler and visible rows | x |
| 2 | Fire (or OneShot / Clip Play override) changes the ruler clip. Anatomy switches to that clip's tracks. Bone groups all re-expand | x |
| 3 | Type a bone name in the filter. Unmatched groups hide. Document and AnimationClip stay clean | x |
| 4 | Collapse a bone group. Fold is session-only. Changing the ruler clip expands every group. Document stays clean | x |
| 5 | Drag in the timeline column (ruler + key lanes) seeks. Clicking a diamond does not select or drag a key. Clicking name / filter / fold does not seek | x |
| 6 | Clip bones missing from the Skeleton still appear, no warning. Select an Object with no AnimationTree: panel stays open, disabled, no anatomy | x |
