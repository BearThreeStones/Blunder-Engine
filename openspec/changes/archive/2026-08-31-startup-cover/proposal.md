## Why

A windowed Editor Session shows an empty HWND (black) from window create until the Editor Shell first presents, and a long silent wait before that while cook and other boot work run with no window. That reads as a broken launch.

## User stories

1. I open a windowed Editor Session. From the first window I see Editor Theme, the wordmark with the Project display name, and a short English stage name — until the Editor Shell is on screen. I do not see a black HWND.
2. Cook or other boot work takes a while. The stage name changes among **Cooking assets**, **Preparing editor**, and **Starting editor**, so the brand field does not look frozen.
3. While the cover is up I close the window. This Editor Session ends with no confirm dialog.
4. Boot is already warm. The cover may flash briefly; the Shell appears as soon as it is ready, with no extra wait.
5. I launch Headless, CLI, or MCP Editor, or Project Manager, or Player. There is no Startup cover.
6. Boot fails fatally while the cover is up. The process ends as it does today. The cover is not an error page and there is no Retry.

## What Changes

- Show a **Startup cover** on the same OS window that becomes the Editor Session, from the first moment that window can show (after the Project File yields the Project display name, still before cook and Vulkan) until the Editor Shell is on screen.
- Paint Editor Theme Window (Base 2), centered Application Bar wordmark, and a short English stage name. No percent bar, splash illustration, or Logo asset.
- Keep that window a normal OS window (close aborts boot with no confirm; minimize and resize stay available). Pump events during remaining boot so it stays responsive.
- Dismiss on first Shell present with no minimum dwell. Fatal boot does not become a cover error UI.
- **Out of scope:** Project Manager, Player, Headless / CLI / MCP, covering until Live scene or first 3D frame, a second splash window, Light theme.

## Capabilities

### New Capabilities

- `startup-cover`: Windowed Editor Session branded cover on the session window until the Editor Shell is on screen.

### Modified Capabilities

- (none)

## Impact

- Boot order in `RuntimeGlobalContext::startSystems`: create the session window after Project File identity, paint the cover, then cook / remaining Systems / Vulkan / Slint behind it.
- Native client paint on the HWND until Slint owns Present (cannot use Editor Shell Slint before the shared Vulkan device exists).
- Event pump during blocking boot so close / resize / WM_PAINT work.
- Docs already grilled: `CONTEXT.md` **Startup cover**, [ADR 0052](../../../docs/adr/0052-startup-cover-same-window.md).
- Tests: host gating and stage sequencing without a window; windowed feel is Human acceptance. Fallback validation is an `engine_editor` build when no distinctive test stem matches.
- No Slint fork change. No Player / Project Manager cover. No Headless window.
