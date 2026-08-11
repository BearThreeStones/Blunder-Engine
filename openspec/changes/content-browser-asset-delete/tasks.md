## 1. Delete service (TDD)

- [x] 1.1 TDD: delete Mesh with no dependents — descriptor gone, GUID unregistered, Intermediate `source` removed
- [ ] 1.2 TDD: delete refused when dependency graph has dependents
- [ ] 1.3 TDD: delete Texture / AnimationClip paths; Mesh delete does not cascade clip Assets
- [ ] 1.4 TDD: Mesh/Texture Final marked stale or cooked output removed after delete

## 2. Content Browser UI

- [x] 2.1 Grid selection highlight for descriptor items
- [x] 2.2 Delete key and/or context menu invokes delete for selected descriptor
- [x] 2.3 Wire UiEvent → delete service → browser refresh + user-visible refuse message

## 3. Integration / docs

- [ ] 3.1 Manual smoke: Import Chocomel Mesh → Delete → re-import cleanly
- [ ] 3.2 Update CONTEXT / agent notes only if glossary terms drift (prefer no churn)
