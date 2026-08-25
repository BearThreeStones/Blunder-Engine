## 1. Row icon payload

- [x] 1.1 Add ordered icon slots on `EditorHierarchyTreeRow` / `HierarchyTreeRow` (kind + Behaviour/Modifier index)
- [x] 1.2 Fill slots in `HierarchySystem::appendVisibleSubtree` from the same sources Inspector uses: Transform always; MeshRenderer; Camera; Light; Skeleton; AnimationTree; Behaviours; SkeletonModifiers
- [x] 1.3 Never emit AnimationPlayer or Clip Binding slots even if the Object still has a Player or a non-empty Tree clip map
- [x] 1.4 Copy the same slot list in `dock_floating_window_host.cpp` so floating Hierarchy matches docked
- [x] 1.5 Test (or rebuild smoke): empty entity → Transform only; Camera+Light+Tree present; two Behaviours keep list order; Player present → no Player icon; clip map non-empty → no clip icons

## 2. Hierarchy strip and hit testing

- [x] 2.1 Right-align the icon strip in `hierarchy.slint`; Transform uses tool-move; MeshRenderer uses `EditorIconMesh`; Uniques/Behaviour/Modifier reuse existing Add… kind wrappers; wrap `EditorGlyphs.tool-move` if no `EditorIcon*` yet
- [x] 2.2 Per-icon `TouchArea`: LMB without Alt → `entity-selected` only; Alt+LMB → select + preview `(entity-id, kind, index)`; right-down → same row context menu as the rest of the row (no preview, no expand)
- [x] 2.3 Detect Alt via Slint `PointerEvent` modifiers if present, else C++ Alt key state on the click callback
- [x] 2.4 Name elides; chevron/gutter/Create… unchanged; scene title chrome has no icons
- [x] 2.5 Manual: docked + floating Hierarchy; LMB icon selects; LMB chevron still toggles; Alt+LMB name does not open a card

## 3. Attachment property preview

- [x] 3.1 Host floating cards on `editor_window` (visible when Hierarchy is docked or floating); identity `(entity-id, kind, index)`
- [x] 3.2 Card body shows the same authorable fields as that Inspector section (Transform, MeshRenderer, Unique, Behaviour, Modifier)
- [x] 3.3 Pin locks the card across selection change (not dock auto-hide pin). Unpinned: other-entity selection or Alt+LMB another icon closes it. Unpinned same-icon Alt+LMB closes; pinned same-icon Alt+LMB raises, no duplicate
- [x] 3.4 Close the card when the entity is deleted or that attachment is Removed. Undo restore does not reopen. `openScene` / document swap closes all cards. Entering Play Mode does not close cards
- [x] 3.5 Camera Unique icon Alt+LMB opens the Camera field card and does not open, close, or retarget Camera Preview
- [x] 3.6 Manual: pin vs unpin; two pinned cards; Remove Camera closes that card; switch scene clears cards; Play leaves cards up; Camera Preview still follows selection

## 4. Document History

- [x] 4.1 Preview field commits use the same Inspector Document History Commands (including pinned card whose entity is not the current selection)
- [x] 4.2 When a preview card has input focus, Focus-routed Undo targets Document History (not Asset Inspector Global)
- [x] 4.3 Test: commit from a pinned card → Document History undo restores; Ctrl+Z with preview focus does not pop Global History

## 5. Validate

- [x] 5.1 Confirm CONTEXT **Hierarchy row icons** and **Attachment property preview** match the shipped gestures
- [x] 5.2 Build `engine_editor` (`docs/agents/build.md`); kill `engine_editor` / `engine_player` if the Player POST_BUILD copy is locked
- [x] 5.3 Run tests from 1.5 and 4.3
- [x] 5.4 Manual: empty row still has Transform; no Player/clip icons; docked + floating; LMB vs Alt+LMB; pin; Document History; Camera Preview collision
