## 1. Viewport coordinate fix

- [x] 1.1 Store logical + render viewport sizes on `EditorCamera`; fix `viewportLocalToNdc` to use logical dimensions
- [x] 1.2 Add `windowToViewportRenderPixel()` and use it in `PickOverlay` for ID/depth readback
- [x] 1.3 Pass logical viewport size from `RenderSystem::setViewportRect`

## 2. Piercing menu fix

- [x] 2.1 Anchor menu with `mapWindowPointerToLogical` in `SlintSystem::showPiercingMenu`
- [x] 2.2 Fix `piercing_menu.slint` hit-testing (Text inside TouchArea, panel blocks backdrop)

## 3. Depth peel verification

- [x] 3.1 Align peel discard epsilon with D32 readback; avoid duplicate-entity false stop on same depth plane
- [x] 3.2 Build `engine_runtime` / `engine_editor`
