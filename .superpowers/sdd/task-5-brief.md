### Task 5: Preflight gates (OpenSpec 5.1–5.3)

**Workspace:** E:/Dev/Blunder-Engine/.worktrees/play-mode-ui

**OpenSpec:**
- 5.1 Dirty scene prompt: Save and Play / Play last saved / Cancel
- 5.2 Scripts dirty detection + ScriptsBuilder/`dotnet build` before spawn; fail → stay Stopped
- 5.3 Tests for dirty prompt decision paths and scripts-dirty build gate (fakes OK)

**TDD mandatory** for decision helpers / gates with fakes before wiring UI.

Integrate with PlaySessionController: preflight runs before spawn. Dirty prompt can be a pure decision function + Slint dialog if pattern exists; otherwise controller API + binder callbacks.

**Commit:** `feat(editor): Play dirty-scene and Scripts build preflight`

Mark 5.1–5.3.

**Out of scope:** demote editor DotNetHost (Task 6); full e2e smoke (Task 7).
