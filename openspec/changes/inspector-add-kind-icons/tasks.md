## 1. Icon wrappers

- [x] 1.1 Add `EditorIconCamera3D` / `EditorIconSkeleton3D` / `EditorIconAnimationPlayer` / `EditorIconAnimationTree` / `EditorIconScript` / `EditorIconSkeletonModifier3D` in `editor_icons.slint` (14px, `colorize`)

## 2. Inspector placement

- [x] 2.1 Unique section headers: icon between expand arrow and title; `icon-color` matches title
- [x] 2.2 Behaviour and SkeletonModifier rows: icon left of type name; missing Behaviour uses `#ff9a9a`; no icon on those section titles
- [x] 2.3 Add… picker kind rows: same six icons; Unique present uses `#808080`; HorizontalBox instead of absolute `x: 8px` text; group labels stay text-only

## 3. Validation

- [x] 3.1 Confirm CONTEXT **Add… kind icon** matches shipped placements (no extra glossary churn)
- [x] 3.2 Build `engine_editor`
- [ ] 3.3 Manual: mesh-only Add… shows six kind icons; Camera already present greys Camera icon; add Behaviour + missing type tints Script icon; Unique headers show icons; clip rows have none
