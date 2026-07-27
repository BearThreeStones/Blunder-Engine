## 1. Policy helper (TDD)

- [x] 1.1 Add failing `editor_overlay_policy_test` for `editorOverlaysEnabled(Editor|Player)`
- [x] 1.2 Add `editor_overlay_policy.h` and make the test pass
- [x] 1.3 Register CTest target in `engine/src/tests/CMakeLists.txt`

## 2. OverlaySystem gate

- [x] 2.1 Disable authorship overlays and early-return sync/draw when Player host
- [x] 2.2 Smoke: Player presents without grid / Transform / Navigate

## 3. RenderSystem input gate

- [ ] 3.1 Skip Transform / Navigate event handling when Player host
- [ ] 3.2 Manual: editor overlays still work; Player ignores gizmo clicks; Pause still hidden

## 4. Docs

- [ ] 4.1 Confirm `CONTEXT.md` **Editor Overlay** glossary matches behavior
