## Why

Host observation v1 can Capture and Play-frame only on windowed Editor/Player. Agents and CI still need an OS window, which ADR 0043 already rejected as the Headless shape. Windowed close-as-Stop does not apply when there is no window.

## What Changes

- Boot **Headless** as a flag on existing `EngineHostMode::Editor` / `EngineHostMode::Player` (CLI `--headless` on `engine_editor` and `engine_player`). **Not** a third host mode, **not** `engine_agent`
- **Headless Editor** omits Slint, UiHost, and the viewport sink/bridge. It still mounts Authorship System, Play Session, and the Scene still / Capture path
- **Headless Player** has no OS window. Play frame is CPU readback of the Play-rule offscreen color target (same API as v1). Session ends via Stop on the Play control channel or process exit — not window close
- Capture / Play step / Play frame stay the v1 contract. No second observation API. No HWND or hidden-window fake
- Windowed Editor/Player behavior stays as today

**Out of scope:** CLI / MCP adapters; Play dump; `EngineHostMode::Headless`; `engine_agent`; HWND / hidden HWND as Headless; changing Capture aspect or Play step dt; Observe as Authorship intent

## Capabilities

### New Capabilities
- `headless-host`: Editor or Player with no OS window; composition (omit Slint/UiHost/viewport sink on Editor; no Player window); not a third `EngineHostMode`

### Modified Capabilities
- `host-observation`: Headless uses the same Capture / Play step / Play frame; Capture does not require Slint or an OS window
- `play-player`: Headless Player has no window; window-close-ends-session applies to windowed Player only; Headless ends via Stop or process exit; Play frame from offscreen, not HWND
- `play-mode`: Headless Editor still runs a Play session (`PlaySessionController` + Play control channel) without UiHost / Slint Play controls

## Impact

- `engine_editor` / `engine_player` launch parsing (`editor_launch`, `player_launch`)
- `RuntimeGlobalContext::startSystems`: skip SDL window + Slint/UiHost/viewport when Headless Editor; skip SDL window + present when Headless Player; still create Vulkan device + offscreen
- `WindowSystem` / `RenderSystem` Vulkan init: Headless path with no `VkSurfaceKHR` / swapchain present
- `PlaySessionController` mounts in Headless Editor; spawn Headless Player with `--headless`
- Tests: boot composition (no window, Authorship present / absent); Headless Capture; Headless Play step + Play frame via IPC
- Docs: CONTEXT Headless / ADR 0043 already match; no extra glossary terms
