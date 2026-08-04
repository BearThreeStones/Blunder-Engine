# Task 3.1 Report — SkeletonAttachModifier host bone → child Object Transform

## Status
**Done.** `SkeletonAttachModifier` ClassDB product copies host attachment bone world TRS onto a configured child Object's Transform after skeleton sample / modifier apply.

## Commit
`feat(animation): SkeletonAttachModifier copies bone world transform to child Object (task 3.1)`

## Tests
`dogwalk_phase6_skeleton_attach_test: all passed` (2 cases: direct apply + post-AnimationPlayer sample chain)

## Implementation summary
- **Class:** `SkeletonAttachModifier` (`skeleton_attach_modifier.h/.cpp`)
- **ClassDB:** `bone_name`, `child_object_id` (Int ↔ `ObjectId`)
- **Factory:** `Object::addSkeletonAttachModifier()`
- **Apply:** decomposes `getBoneGlobalPoseMatrix(bone)` → `child->setPosition/Rotation/Scale`
- **Wiring:** registration, `class_db.cpp`, runtime + test CMake

## Semantics
- Writes **child Object Transform** only — does not touch another Object's Skeleton poses.
- Runs in existing post-sample modifier chain via `Object::applySkeletonModifiers`.
- Child should be parented under host so bone-space TRS maps to local Object Transform.

## Concerns / deferred (3.2–3.4)
- **3.2:** invalid child/bone currently no-op silently — need error/log path.
- **3.3:** no explicit guard test that remote Skeleton is untouched.
- **3.4:** `AnimationPreviewController` edit-scrub for Attach not added yet.
- **Serialize / Inspector / C-ABI:** tasks 4.x / 5.x still open.
