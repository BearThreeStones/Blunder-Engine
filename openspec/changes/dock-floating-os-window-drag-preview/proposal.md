## Why

Dock floating today uses in-host Slint rectangles (`docking-floatings` inside `docking_host`): panels only float within the main editor window, and tab drag shows dock guides without a semi-transparent “ghost” following the cursor. This diverges from Qt ADS-style UX (real `CFloatingDockContainer` windows + `CFloatingDragPreview`) and feels constrained compared to professional DCC editors. Users expect to drag a tab out, see a preview window track the mouse, and end with a true OS-level floating panel they can move outside the main window bounds.

## What Changes

- Introduce **Non-Opaque drag preview**: while dragging a dock tab (before drop), show a semi-transparent floating preview rectangle that follows the pointer, with frame and title (content pixmap optional in v1).
- Promote committed float state from in-host chrome to **native OS child windows** (Windows first): each floating dock container gets an SDL borderless/popup `SDL_Window` owned by `WindowSystem`, parented to the main editor HWND.
- Add a `DockFloatingWindowHost` (or equivalent) layer between `DockManager` and `SlintSystem` to create/destroy/move/resize native windows and route input per float.
- **Viewport exception (v1):** `DockPanelKind::viewport` SHALL remain in the main Slint tree (Vulkan zero-copy viewport cannot move to a separate present path in this milestone).
- Extend tab-drag lifecycle: press → move past threshold → show preview → release → dock drop **or** commit native float **or** cancel.
- Remove or bypass in-host `docking-floatings` chrome for panels that use OS windows; keep in-host fallback behind a feature flag for non-Windows / debug.
- Wire drop overlays (guides) to track pointer while preview is active, including when cursor is over the main dock host but preview extends outside.

## Capabilities

### New Capabilities

- `dock-floating-os-window`: Native OS floating windows for committed dock float state (create, move, resize, z-order, close, re-dock); Windows/SDL integration; viewport excluded in v1.
- `dock-floating-drag-preview`: Non-Opaque semi-transparent drag preview during tab drag (threshold, follow pointer, cancel, commit on release).

### Modified Capabilities

- _(none)_ — `docking-auto-hide` behavior unchanged; floating may later drop to auto-hide strips as follow-up.

## Impact

- **Docking C++:** `DockManager`, `DockDragController`, new `dock_floating_window_host.*`, layout model changes (`DockFloatingView` may gain `native_window_id` / `in_host_fallback`).
- **Platform:** `WindowSystem` multi-window support (create/destroy child SDL windows, HWND parent, event routing).
- **Slint:** `editor_window.slint` — new `DockFloatingDragPreviewLayer`; reduce/replace in-host `docking-floatings` for native-backed panels; per-float Slint `ComponentHandle` or embedded panel subtree sync.
- **SlintSystem:** `syncDockingWorkspace`, input routing, coordinate transforms (screen ↔ dock-local), drag state machine integration.
- **Render:** Viewport rect cache must ignore OS-floated non-viewport panels; viewport stays on main window Image path.
- **Risk:** High — cross-cutting UI/platform/render; Windows-first; Linux/macOS may keep in-host fallback until later.
