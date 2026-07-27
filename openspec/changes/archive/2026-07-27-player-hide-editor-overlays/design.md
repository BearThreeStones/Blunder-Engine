## Context

Player shares `RenderSystem` / `OverlaySystem` with the editor. Grid and Navigate gizmo force `enabled_ = true` each frame; Transform/Navigate receive events in `RenderSystem::onEvent`. Product glossary (**Editor Overlay**) requires Player to never show or hit-test that chrome.

## Goals / Non-Goals

**Goals**
- Gate authorship overlays on `EngineHostMode::Player`.
- Cover draw and gizmo input.
- Unit-test the pure policy helper (TDD).

**Non-Goals**
- Env override to force overlays in Player.
- Hiding editor viewport overlays during a Play Session.
- Skipping OverlaySystem GPU init in Player.
- Changing Editor Camera orbit.

## Decisions

1. **Policy helper `editorOverlaysEnabled(EngineHostMode)`** — same style as `play_tick_gate.h`; host-mode only (Pause orthogonal).
2. **Central gate in `OverlaySystem`** — disable all authorship overlays and early-return draw paths so individual `begin_sync` cannot re-enable.
3. **Gate `RenderSystem::onEvent` Transform/Navigate blocks** — keep F11 / camera orbit untouched.
4. **No env override** this slice.

## Risks / Trade-offs

- [Risk] Empty screen overlay pass still opened — Mitigate: return before `m_screen_pass.begin`.
- [Risk] Pick still runs in Player — Out of scope; not listed as Editor Overlay draw chrome for this slice.

## Migration Plan

Ship with Player rebuild. Rollback: revert policy gate.

## Open Questions

None — grilled and confirmed.
