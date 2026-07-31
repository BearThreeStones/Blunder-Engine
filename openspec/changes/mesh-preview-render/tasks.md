## 1. Mesh Preview Render core

- [x] 1.1 Add Mesh Preview Render service API (load Mesh Final→Fast Path, frame AABB, studio lights, bind-pose for skinned) with unit/integration hooks for success/fail
- [x] 1.2 Allocate dedicated offscreen RT + readback path (not Camera Preview / main viewport RT); document ownership next to Camera Preview
- [ ] 1.3 Draw all submeshes with materials into RT; return CPU image buffer suitable for Slint/thumbnail PNG
- [ ] 1.4 Tests: Final preferred over Intermediate; Intermediate used when Final missing; failure returns clear error (placeholder allowed upstream)

## 2. Content Browser Mesh thumbnails

- [ ] 2.1 Replace Mesh `ThumbnailGenerator` base-color shortcut with Mesh Preview Render still on success
- [ ] 2.2 Async generation queue with visible-grid priority; keep Texture path unchanged
- [ ] 2.3 On successful regenerate, cache stores 3D still (invalidate/replace old texture-based Mesh cache entries via existing mtime rules)
- [ ] 2.4 Tests: Mesh thumbnail path prefers 3D still; Texture path unchanged; failure → Mesh placeholder not base-color success

## 3. Asset Inspector + Mesh Preview UI

- [ ] 3.1 Content Browser Mesh selection enters Asset Inspector mode (identity: name, GUID, type, Intermediate path)
- [ ] 3.2 Embed interactive Mesh Preview (dedicated RT frames → Slint image); LMB orbit, wheel zoom, double-click reset; ephemeral camera
- [ ] 3.3 Entity selection (viewport/Hierarchy) exits Asset mode and restores Entity Inspector
- [ ] 3.4 Non-Mesh Browser selection does not open Mesh Preview
- [ ] 3.5 Tests: selection mode transitions; orbit not persisted; pointer capture on preview

## 4. Docs / validation

- [ ] 4.1 Confirm CONTEXT terms + ADR 0022 stay aligned with implementation
- [ ] 4.2 Manual check: Sponza (or multi-material Mesh) thumbnail + Inspector preview show framed geometry, not atlas-only
- [ ] 4.3 `/validate` relevant targets (thumbnail/inspector tests + smoke as available)
