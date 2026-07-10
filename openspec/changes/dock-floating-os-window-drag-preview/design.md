## Context

Blunder docking already supports logical floats (`DockNodeKind::floating`, `m_floating_nodes`, `makeFloatingFor` on tab release) rendered as in-host Slint rectangles (`docking-floatings` in `editor_window.slint`). Tab drag uses `DockDragController` with guide preview (`DockPreview`) but no ghost window during move. The user wants Qt ADS parity: **Non-Opaque** `CFloatingDragPreview` during drag and **real OS windows** (`CFloatingDockContainer`) after commit.

Constraints from the render pipeline:
- Viewport 3D uses Vulkan offscreen readback → Slint `Image` on the **main** `SkiaRenderer` HWND (zero-copy path). Moving Viewport to a second present surface is out of scope for v1.
- Single `WindowSystem` today owns one `SDL_Window`; no child-window API yet.
- Slint `SlintSystem` drives one `MainEditorWindow` component; multi-window Slint is possible via multiple `WindowAdapter` instances but is unproven in this repo.

## Goals / Non-Goals

**Goals:**

- Semi-transparent drag preview following pointer after threshold (Non-Opaque mode).
- Committed floats for non-viewport panels as Win32 child SDL windows (borderless + custom title bar in Slint).
- Preserve existing dock tree, guide drop, auto-hide, and re-dock semantics.
- Feature flag to fall back to current in-host floats.

**Non-Goals:**

- Viewport in native OS float (v1).
- Linux Wayland `xdg_toplevel_drag_v1` / macOS native title-bar parity (follow-up).
- Layout serialization / session restore of float positions.
- Float entire multi-tab DockArea in one gesture (single-widget float only, same as today).
- Content pixmap inside drag preview (title + frame sufficient for v1).

## Decisions

### 1. Two-phase float: preview (in-host) → commit (OS window)

**Choice:** Drag preview stays in the **main** Slint tree (`DockFloatingDragPreviewLayer`). Only on mouse release without guide drop do we call `makeFloatingFor` and spawn `DockFloatingWindowHost`.

**Rationale:** Matches ADS Non-Opaque then commit; avoids creating/destroying OS windows on every drag twitch.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Opaque mode (OS window immediately on threshold) | Higher flicker; harder to cancel; more HWND churn |
| Preview as OS window | Same churn; transparency on layered HWND is fragile |

### 2. `DockFloatingWindowHost` owns SDL child windows

**Choice:** New module between `DockManager` and `SlintSystem`:

```
DockManager (tree, rects, drag)
       ↓ layout model
DockFloatingWindowHost (SDL_Window* per float, screen coords)
       ↓
SlintSystem (main window + optional per-float Slint surfaces OR panel delegate)
```

Each float gets:
- `SDL_CreateWindow` with `SDL_WINDOW_BORDERLESS` (and popup/tooltip flags as needed)
- Win32: parent via `SDL_SetProperty` / `SetWindowLongPtr(GWLP_HWNDPARENT)` to main HWND
- Title bar + grip in a **dedicated small Slint window** (`FloatingPanelWindow.slint`) OR reuse panel subtree rendered to child Skia surface

**Rationale:** Keeps `DockManager` UI-agnostic; centralizes HWND lifetime.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Extend `WindowSystem` only | WindowSystem is engine-global; docking-specific policy doesn't belong there bare |
| Keep in-host only | User requirement explicitly rejects this |

### 3. Per-float Slint surface (Windows v1)

**Choice:** One lightweight Slint `Window` per native float hosting Hierarchy / Inspector / Content Browser subtrees. Main editor stops rendering those panel rects when `native_window_id` is set.

**Rationale:** Panel widgets already exist as Slint components; child Skia present is simpler than cross-HWND blit from main composite.

**Risk:** Second Slint renderer may not share Vulkan device with viewport — acceptable because viewport stays on main window.

### 4. Extend `DockDragController` state machine

**Choice:** Add states `preview_active` (and optionally `preview_cancelled`). Store `preview_rect`, `drag_start_pointer`, `threshold_px` (default 8). `SlintSystem` reads preview from layout model extension `DockDragPreviewView`.

**Rationale:** Minimal change to `DockManager::endDrag` contract; preview is drag concern not tree concern.

### 5. Viewport float policy

**Choice:** `makeFloatingFor` checks `DockPanelKind::viewport` → if native OS enabled, use **in-host float only** (current `docking-floatings` path) or reject float entirely (design: **in-host float allowed** so Viewport can still detach visually inside main window).

**Rationale:** User gets float UX for Viewport without breaking render pipeline.

### 6. Coordinate spaces

| Space | Use |
|-------|-----|
| Dock-local | Guides, tiles, in-host preview |
| Main client | `DockManager::m_host_rect`, stored `floatingRect` |
| Screen | SDL `SDL_SetWindowPosition` for child windows |

Conversion: `ClientToScreen` on main HWND for drag release; inverse on float move to update `floatingRect`.

### 7. Input routing

- Main window: dock guides, main tiles, drag preview (no hit-test on preview — pointer events pass through conceptually; preview is visual only).
- Child float windows: SDL events routed to that float's Slint adapter; title drag calls `DockManager::beginFloatingMove` with screen→client conversion.
- `SlintSystem::dispatchApplicationEvent` dispatches by `SDL_WindowID`.

## Architecture sketch

```
 Tab press
     │
     ▼
 move > threshold ──► DockDragPreviewView ──► Slint DragPreviewLayer (main HWND)
     │                      │
     │                      └── pointer drives handleDragMove (guides)
     ▼
 release ──┬── guide hit ──► dockWidget (preview off)
           ├── float eligible ──► makeFloatingFor
           │         │
           │         ├── viewport ──► in-host docking-floatings
           │         └── other ──► DockFloatingWindowHost::create
           │                        └─ SDL child + Slint FloatingPanelWindow
           └── cancel (Esc) ──► preview off, no op
```

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Multi-Slint-window Skia init cost | Lazy-create renderer on first float; pool/destroy on close |
| HWND Z-order vs main window | Raise float on activation; `SetWindowPos(HWND_TOP)` |
| DPI / mixed scaling | Use SDL logical size + `getDisplayScale` on both windows |
| Event storms during drag | Coalesce pointer moves (existing pattern in `SlintSystem`) |
| Viewport special case confusion | Document in UI; optional pin Viewport non-float |
| Linux/macOS unsupported | Feature flag → in-host fallback |

## Migration Plan

1. **Phase A — Drag preview only** (lower risk): `DockDragPreviewView` + Slint layer; no OS windows yet.
2. **Phase B — OS window host**: `WindowSystem` child API + `DockFloatingWindowHost` + Inspector/Hierarchy float.
3. **Phase C — Content Browser float + polish**: close button, z-order click, escape cancel.
4. Rollback: disable `native_os_window` flag → revert to `docking-floatings` only.

## Open Questions

- Can Slint 1.16 fork spawn multiple `SkiaRenderer` instances on child HWNDs sharing no Vulkan with main? **Spike required before Phase B.**
- Parent HWND: SDL3 child window parenting API on Win32 — verify with minimal SDL sample.
- Should drag preview use a static size or measured tab/container size? **Default: use source container content rect size at drag start.**
