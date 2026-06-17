# Validate Blunder change

Run the default validation workflow for first-party engine changes.

## Steps

1. Read [docs/agents/common-tasks.md](docs/agents/common-tasks.md) for task-specific extras.
2. Build the editor:
   ```powershell
   cmake --build build/vs2026-debug --config Debug --target engine_editor
   ```
3. Run the editor and exercise the affected UI or scene path:
   ```powershell
   ./build/vs2026-debug/engine/src/editor/Debug/engine_editor.exe
   ```
4. If meshes, textures, or import pipeline changed: re-cook per [CONTENT_LAYOUT.md](CONTENT_LAYOUT.md) or run `asset_compiler --force`.

## Report

- Build succeeded or failed (include relevant errors).
- What was manually verified in the editor.
- Any follow-up needed (re-cook, Slint rebuild, etc.).
