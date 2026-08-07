## 1. Spec wiring & docs

- [x] 1.1 CONTEXT Scene Thumbnail Render + ADR 0024 (grill)
- [x] 1.2 Update CONTENT_LAYOUT.md thumbnail sources list for `.scene.asset`

## 2. Pure helpers (TDD first)

- [x] 2.1 `scene_thumbnail_fingerprint` — collect direct mesh refs (root + recursive childScenes) + hash with mtimes
- [x] 2.2 Unit test: fingerprint changes when child mesh mtime changes; stable when unrelated files change
- [x] 2.3 Unit test: Play camera resolve preference for scene thumbnail framing helper (Main vs first)

## 3. Placeholder & cache

- [x] 3.1 Add `ThumbnailPlaceholderKind::Scene` + draw icon
- [x] 3.2 ThumbnailCache: `.scene.asset` stem suffix `_scv1` + fingerprint segment for invalidation
- [x] 3.3 ThumbnailGenerator: route `.scene.asset` → scene generate / Scene placeholder on failure

## 4. Scene Thumbnail Render service

- [x] 4.1 Private temporary SceneInstance tree from on-disk asset (recursive childScenes, mesh attach, cameras)
- [x] 4.2 Resolve Play camera at aspect 1; build MeshPreviewCameraFrame; studio lights fallback
- [x] 4.3 Collect mesh draws (world matrices) without Editor Overlays / AnimationPlayer sample
- [x] 4.4 Extend Mesh Preview offscreen backend with multi-draw + explicit camera frame (distinct SceneThumbnail owner)
- [x] 4.5 Wire service into ThumbnailGenerator / global_context like Mesh Preview

## 5. Verification

- [x] 5.1 `scene_thumbnail_fingerprint_test` (and/or extend thumbnail_generator_test) passes
- [x] 5.2 Build `engine_editor` (manual Browser smoke for dogwalk_test_rig still recommended)
