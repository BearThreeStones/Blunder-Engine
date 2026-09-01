# Manual checklist — animation-window-preview-clip

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Passed 2026-08-31 (human walked windowed `engine_editor`)

Windowed editor. Bind Chocomel (AnimationTree) unless a row says otherwise.

| # | User story | Pass |
|---|------------|------|
| 1 | Select Chocomel. Preview clip defaults to walk. Window Clip Plays walk and stays at t=0. Viewport, ruler, and Clip anatomy show walk. Document stays clean | x |
| 2 | Press Play. Walk advances. While Playing, change Preview clip to idle. Idle hard-cuts at t=0. Viewport, ruler, and anatomy follow idle | x |
| 3 | Pause or Stop, then change Preview clip to another clip. That clip Clip Plays at t=0 and does not start transport. Stop does not clear Clip Play override. Ruler stays on that clip at frame 0 | x |
| 4 | Preview clip is idle. Press window Fire. Idle occupies Fire slot; ruler follows insert. While Fire occupies, change Preview clip to walk. Fire keeps playing; Clip Play base underneath is walk. When Fire ends, ruler shows walk | x |
| 5 | Select a different Object. Previous tree's Clip Play override is cleared. New Object with AnimationTree: Preview clip is its default, Clip Play at t=0 without playing. No Tree: panel stays open and disabled, no Clip Play or anatomy | x |
