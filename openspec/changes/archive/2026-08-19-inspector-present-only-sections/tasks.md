## 1. Inspector present-only UI

- [x] 1.1 Wrap Camera / Skeleton / Animation Player / Animation Tree header+body+divider in `if has-*`; delete `No Skeleton on entity` / `No AnimationPlayer on entity` (and any Camera/Tree empty placeholders)
- [x] 1.2 Wrap Behaviours and Skeleton Modifiers sections the same way on `behaviours.length > 0` / `skeleton-modifiers.length > 0`
- [x] 1.3 Confirm Add… Unique rows still list and disable when present (no catalog hide)

## 2. Docs / validation

- [x] 2.1 CONTEXT glossary: present-only Inspector sections (property surface, not Add… catalog)
- [x] 2.2 Build `engine_editor`
- [x] 2.3 Manual: mesh-only entity shows Add… + Transform, no Unique/Behaviours/Modifier slots; Add… AnimationPlayer reveals Skeleton + Player; Remove Player hides Player, keeps Skeleton; last Behaviour Remove hides Behaviours
