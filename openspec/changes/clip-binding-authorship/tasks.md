## 1. Binding invariants

- [x] 1.1 Enforce unique logical names on append/rename commit (reject collision; no last-write-wins)
- [x] 1.2 On scene load / apply bindings, discard dual-empty name+GUID rows; keep half-filled as invalid
- [x] 1.3 Tests: duplicate append/rename rejected; dual-empty stripped on load; half-filled preserved

## 2. AnimationPlayer Inspector surface

- [x] 2.1 Replace GUID LineEdit with AnimationClip identity display + per-row picker affordance; keep editable logical name + Remove
- [x] 2.2 Add clip opens AnimationClip picker; confirm appends complete binding (stem default); cancel no-ops and pushes no History
- [x] 2.3 Per-row picker retargets Asset Reference and keeps logical name; History via clip-bindings Command
- [x] 2.4 Update/remove empty-draft Add clip paths and tests that expect empty name→GUID rows

## 3. Content Browser drop

- [x] 3.1 Classify AnimationClip drops (extend ContentBrowserDropKind or equivalent)
- [x] 3.2 Drop on empty list / Add clip area → append binding (stem default; uniqueness reject)
- [x] 3.3 Drop on existing row → retarget that row only (keep name); History Command
- [x] 3.4 Tests: append vs retarget; non-clip drop rejected

## 4. Behaviour clip-name fields

- [x] 4.1 Catalog/metadata mark for Behaviour clip-name members (attribute or agreed mark → catalog field)
- [x] 4.2 Inspector: marked strings render as dropdown of co-located player logical names + empty; unmarked strings stay free text
- [x] 4.3 Invalid styling when stored name missing from map; no cascade on binding rename/remove; drop-on-Behaviour does not mutate map
- [x] 4.4 Tests: dropdown options; empty-map only empty choice; weak ref survives rename

## 5. Docs / validation

- [x] 5.1 Confirm CONTEXT Clip Binding / Behaviour clip name and ADR 0036 match shipped behavior
- [x] 5.2 Build `engine_editor`; run focused clip-map / Add clip / drop / Behaviour tests from 1.3, 2.4, 3.4, 4.4
- [ ] 5.3 Manual: Add clip / drop LOOP-chocomel-idle & walk → optional rename → PlayerMove IdleClip/WalkClip dropdown → Play idle↔walk
