# Task 2.2 Report — Async visible-first thumbnail queue

## Status
Complete.

## Summary
Added `ThumbnailGenerationQueue` with `Visible` vs `Background` priority. `ThumbnailGenerator` exposes `probeThumbnailStatus`, `enqueueThumbnail`, and `tickThumbnailQueue`. `ContentBrowserSystem::refresh()` scans + probes cache only, enqueues missing entries as background, promotes visible grid items after `rebuildGrid()`, and drains the queue via `tickThumbnailQueue()` (2/frame) from `engine.cpp` with Slint resync. Texture thumbnails still use `generateImageThumbnail`; mesh path unchanged from 2.1.

## Tests (TDD)
- RED: `thumbnail_generation_queue_test` — visible mesh processed before off-screen; texture path skips mesh preview.
- GREEN: implementation above.
- Regression: `thumbnail_generator_test` (2.1) still passes.

## Files
- `engine/src/runtime/resource/thumbnail/thumbnail_generation_queue.{h,cpp}`
- `engine/src/runtime/resource/thumbnail/thumbnail_generator.{h,cpp}`
- `engine/src/runtime/resource/content_browser/content_browser_system.{h,cpp}`
- `engine/src/runtime/resource/content/content_index.cpp`
- `engine/src/runtime/engine.cpp`
- `engine/src/tests/thumbnail_generation_queue_test.cpp`
- CMake: runtime + tests targets

## Concerns
- Queue drains on main thread (GPU mesh preview requires it); async = non-blocking refresh, not background threads.
- Budget is 2 thumbnails/frame; large folders may take several seconds to fully populate.
- Task 2.3 cache invalidation on regenerate not addressed here.

## tasks.md
2.2 marked `[x]`.

## Fix pass (review)
- **Critical:** `ThumbnailGenerationQueue::demoteAll(Background)` at start of `enqueueVisibleGridThumbnails()` so stale Visible items from prior folder/search are demoted before grid promotion. `refresh()` clears queue before re-enqueue to avoid duplicate/stale backlog.
- **Test:** `demoteAllResetsStaleVisiblePriority` — A Visible, B Background → demoteAll → promote B → B ticks first.
- **Verified:** `thumbnail_generation_queue_test`, `thumbnail_generator_test` pass.
