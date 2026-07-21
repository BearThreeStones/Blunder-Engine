# Play Mode UI (separate Player process)

> OpenSpec: `openspec/changes/play-mode-ui/`  
> Branch: `feat/play-mode-ui`  
> ADR: `docs/adr/0014-play-mode-separate-player-process.md`

## Global Constraints

- **TDD** for every behavior change; watch tests fail first (Superpowers TDD).
- Play Mode = separate **`engine_player`** process (not in-editor simulation). ADR 0014.
- Thin main + shared runtime; no editor Slint shell in Player.
- IPC: localhost TCP ephemeral port default; commands `pause` / `resume` / `stop`; Player emits `ready`.
- Pause = skip Behaviour Tick; render/orbit OK.
- At most one Play session; window close = Stop.
- Dirty scene prompt (Save and Play / last saved / Cancel); Scripts build when dirty.
- Edit Mode does not auto-start DotNetHost; `BLUNDER_DOTNET_SCRIPTS` debug-only.
- No live sync / ALC in this slice.
- Scoped commits; do not commit unrelated WIP (Assets/PM/history/asset-pipeline).
- Mark OpenSpec checkboxes in `openspec/changes/play-mode-ui/tasks.md`.
- Subagent model: `cursor-grok-4.5-high`.
- Workspace: `.worktrees/play-mode-ui` when using SDD.

---

### Task 1: Player executable skeleton (OpenSpec 1.1–1.4)

**Files:** `engine/src/player/` (new), CMake, CLI parse (testable without full boot)

**TDD:**
1. RED: CLI parse test for `--project-root` / `--scene` / `--play-ipc` fails.
2. GREEN: parse helpers + `engine_player` target; boot path loads project + scene (minimal).
3. Commit: `feat(player): add engine_player skeleton and CLI`

Mark 1.1–1.4.

---

### Task 2: Player host + Pause (OpenSpec 2.1–2.4)

**TDD:** Pause flag unit test (Tick skipped when paused) before wiring engine loop.

Commit: `feat(player): DotNetHost mount and Pause tick gate`

Mark 2.1–2.4.

---

### Task 3: Play control channel (OpenSpec 3.1–3.3)

**TDD:** IPC loopback pause/resume/stop + ready before full Player integration.

Commit: `feat(play): localhost IPC control channel`

Mark 3.1–3.3.

---

### Task 4: Editor Play session (OpenSpec 4.1–4.4)

**TDD:** `PlaySessionController` state machine tests with fake process/IPC.

Commit: `feat(editor): PlaySessionController and Play/Pause/Stop UI`

Mark 4.1–4.4.

---

### Task 5: Preflight gates (OpenSpec 5.1–5.3)

**TDD:** Dirty prompt decisions + scripts-dirty build gate with fakes.

Commit: `feat(editor): Play dirty-scene and Scripts build preflight`

Mark 5.1–5.3.

---

### Task 6: Edit Mode host policy + docs (OpenSpec 6.1–6.3)

Commit: `docs(play): demote editor DotNetHost auto-start; document Player`

Mark 6.1–6.3.

---

### Task 7: Verify (OpenSpec 7.1–7.2)

Run session + IPC tests green; smoke or document manual Player Tick path.

Commit if needed: `test(play): verify Play session and IPC`

Mark 7.1–7.2.
