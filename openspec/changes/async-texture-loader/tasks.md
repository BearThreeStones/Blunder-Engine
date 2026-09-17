## 1. CPU request path

- [x] 1.1 Add Texture Loader sources under `engine/src/runtime/function/render/` (request by virtual path, in-flight map, generation id). Submit file read + decode as Jobs into caller-owned Job data; poll done/failed on the tick; do not `wait()` to present.
- [x] 1.2 Wire the Loader onto RenderSystem (Player and Editor). Headless / no device: skip GPU enqueue.

## 2. GPU upload queue

- [x] 2.1 Staging buffer pool: rent CPU-visible buffer, copy, return only after the copy fence. Cap outstanding staging bytes.
- [x] 2.2 Record buffer→image copy, submit on the graphics queue without waiting the tick (`submitImmediateCommandsNoWait` or equivalent). Poll fences each frame. Do not use `endImmediateCommands` for material textures.

## 3. Bindless residency

- [x] 3.1 Until the copy fence signals, material draws keep Bindless fallback index 0. After the fence, `writeSlot` and use the resident index. Stale generation must not `writeSlot`.

## 4. Cancel and shutdown

- [x] 4.1 Scene drop / replace bumps generation (or equivalent); in-flight Jobs may finish but their pixels are dropped. Process shutdown stops new requests, drains or drops GPU fences, `wait()`s remaining CPU Jobs on the owner, and returns without hang.

## 5. Tests and docs

- [x] 5.1 Add `texture_loader_test` (link `engine_runtime` as needed): coalesce same path; cancel/generation drops a completed CPU buffer; Job decode writes Job data. No windowed Slint.
- [x] 5.2 Build and run `texture_loader_test` (`build/vs2026-debug`, Debug).
- [x] 5.3 If code names differ from CONTEXT Texture Loader / ADR 0058, align the glossary only; do not expand v1 into mesh streaming, transfer-queue family, or a second AssetManager.
