## Context

`dock-floating-os-window-drag-preview` (Phase B) introduced native OS child windows for committed floats (`DockFloatingWindowHost`, `ChildWindowRegistry`, `floating_panel_window.slint`) and a global pointer poll (`SlintSystem::tickGlobalDockPointerPoll`) to route drag/resize when the cursor leaves child HWNDs.

Manual validation surfaced three related failures:

| Symptom | User observation | Likely cause |
|---------|------------------|--------------|
| No re-dock from OS float | No guides even when float moved aside | Wrong dock-local coords from poll; dual coordinate paths |
| Guide direction jumps (main window) | Selecting one edge jumps to another or center | Poll overwrites Slint `tab-moved` every frame; brittle `computeSlot` |
| Resize works, cursor static | Edge drag changes size; cursor stays arrow | Slint `mouse-cursor` not propagated to Win32/SDL child adapter |

```
  tab-moved (Slint)          tickGlobalDockPointerPoll (every frame)
         │                              │
         └──────────┬───────────────────┘
                    ▼
           handleDragMove(pointer)
                    │
         ┌──────────┴──────────┐
         ▼                     ▼
     hitTest              computeSlot
         │                     │
         ▼                     ▼
   guides / preview      slot flicker
```

**Constraints:** Windows-first; viewport stays in main window; existing `DockManager` tree semantics unchanged; minimal diff scoped to pointer routing and slot stability.

## Goals / Non-Goals

**Goals:**

- Native-float tab drag back to main dock shows guides, preview, and completes valid drops.
- Main-window tab drags have stable guide highlight and preview rect.
- Native float edge resize shows correct OS cursor (and optional Slint hover tint).
- Single authoritative coordinate path per drag origin (in-host vs native float).

**Non-Goals:**

- Linux/macOS cursor parity beyond best-effort SDL (Windows v1).
- Changing `edge_slot_fraction` default globally without hysteresis (prefer stability over retuning UX).
- Viewport native float or drag-preview content pixmap.
- Layout serialization of float positions.

## Decisions

### 1. Scope `tickGlobalDockPointerPoll` by drag origin

**Choice:** Add `DockManager::dragNeedsGlobalPointerPoll()` (or track `DragOrigin::in_host | native_float` on `beginDrag`). Call `handleDragMove` from poll **only** when origin is `native_float`. Float resize interaction continues to use poll unconditionally.

**Rationale:** In-host drags already receive accurate dock-local coords from `DockTabsLayer` `tab-moved`. Poll + `screenToDockLocal` differs by sub-pixel amounts and overwrites Slint each `beginFrame`, causing `computeSlot` flicker.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Poll for all drags, fix coords only | Still two writers per frame; harder to reason about |
| Disable poll entirely | Breaks native-float tab drag when cursor leaves child HWND |
| SDL_CaptureMouse on main only | Does not fix coordinate mismatch |

### 2. Unified `pointerToDockLocal` for native-float tab drag

**Choice:** New helper (e.g. on `ChildWindowRegistry` or `DockFloatingWindowHost`):

```
pointerToDockLocal(global_screen_pt, docking_origin):
  if cursor over any registered child float client rect:
    return floatLocalToDockLocal(child, local_pt, origin)
  else:
    return screenToDockLocal(screen_pt, origin)
```

Use **only** from `tickGlobalDockPointerPoll` during native-float tab drag. Align scale factors with `mapWindowPointerToLogical` / `mapSdlWindowPointerToLogical` (`window_pointer_map.h`).

**Rationale:** Title-bar move already uses `floatLocalToDockLocal` and works. Tab drag poll used a different global path that diverges under HiDPI. User confirmed failure persists when float is not occluding main window → not a z-order-only issue.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Always `screenToDockLocal` | Proven wrong for re-dock hit test |
| Always child-local from last known window | Fails when cursor is over main client |

### 3. Hide source native float HWND during tab drag

**Choice:** On `beginDrag` when source container is under a native float, call `DockFloatingWindowHost::setFloatVisible(dock_id, false)`. Restore on `cancelDrag` or `endDrag` if float still exists (not destroyed by successful re-dock).

**Rationale:** Removes occlusion of main-window guides/preview; simplifies hitTest (source float already skipped, but hidden window avoids stray Win32 hit targets). Matches Qt ADS pattern of dragging a “ghost” while source chrome is inactive.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Lower float z-order only | HWND may still block; guides behind float |
| Transparent layered window | Fragile on Windows; out of scope |

### 4. Stabilize `computeSlot` with hysteresis

**Choice:** Replace float-equality tie-break (`min_edge == distance_left`) with index-of-minimum on `{left, right, top, bottom}`. Add slot stickiness on `DockDragController`:

- Store `last_slot` and `last_slot_pointer`.
- When computing new slot, if pointer moved less than `hysteresis_px` (e.g. 4 logical px) from `last_slot_pointer`, keep `last_slot`.
- When crossing `edge_slot_fraction` boundary, require pointer to move `hysteresis_fraction` (e.g. 0.02) past boundary before switching center ↔ edge.

**Rationale:** `edge_slot_fraction` (0.25) creates sharp boundaries; 1–2 px jitter from dual pointer sources or float math flips highlight. User report: “选定一个方向会突然跳转到另一方向或中心.”

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Increase `edge_slot_fraction` to 0.3 | Shrinks center drop zone; doesn’t fix corner ties |
| Snap to nearest guide icon rect | Guides are visual only; slot drives drop geometry |
| Debounce in Slint only | C++ owns slot for drop validity |

### 5. OS cursor on native float child windows

**Choice:** In child `WindowAdapter` event path (or `ChildWindowRegistry` Win32 subclass), map `DockResizeEdge` / Slint hover to `SDL_SetCursor` with system resize cursors. Fallback: handle `WM_SETCURSOR` on child HWND when Slint reports edge hover via callback from `floating_panel_window.slint`.

**Rationale:** Resize logic already works (TouchAreas + `updateFloatingInteraction`); only cursor propagation missing — typical for second Slint `WindowAdapter` without cursor bridge.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Slint `mouse-cursor` only | Does not reach Win32 for child adapter (observed) |
| Custom cursor bitmaps | Unnecessary for v1 |

### 6. Optional Slint edge hover tint

**Choice:** Mirror southeast grip `has-hover` styling on N/S/E/W edge strips in `floating_panel_window.slint` (subtle `#3a6ea5` or theme token).

**Rationale:** Accessibility when OS cursor fails on some GPUs/drivers; low-cost complement to Decision 5.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Hiding float during drag confuses users who expect to see source window | Brief drag; restore on cancel; preview on main window remains |
| Hysteresis delays slot switch near boundary | Keep `hysteresis_px` small (4px); tune from manual QA |
| `pointerToDockLocal` still wrong on exotic DPI | Log dock-local vs Slint coords in debug build; align with existing `mapWindowPointerToLogical` tests |
| `WM_SETCURSOR` conflicts with SDL | Prefer SDL API first; Win32 subclass only if needed |
| Poll + `tab-moved` both fire on native float before hide | Ignore `tab-moved` for drag move when `dragNeedsGlobalPointerPoll()` true |

## Migration Plan

1. Land Decision 1 (scoped poll) — immediate main-window guide stability win.
2. Land Decision 4 (`computeSlot` hysteresis) — same PR or follow-up.
3. Land Decisions 2–3 (coords + hide float) — native re-dock.
4. Land Decision 5–6 (cursor + tint) — independent, low risk.
5. Manual validation: tasks 6.x in `tasks.md`; then mark `dock-floating-os-window-drag-preview` validation items complete if satisfied.

**Rollback:** Each decision is locally revertible; feature flag `native_os_window` unchanged.

## Open Questions

- Should `beginDrag` set `SDL_CaptureMouse` on main HWND during native-float tab drag for cleaner `endDrag` on release outside any window? (Spike if drop still fails after coords fix.)
- Exact `hysteresis_px` / `hysteresis_fraction` values — start with constants in `DockMetrics`, tune in QA.
- Archive `dock-floating-os-window` delta into `openspec/specs/` when both changes ship, or keep as change-local delta until parent archives.
