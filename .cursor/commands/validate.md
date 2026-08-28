# Validate Blunder change

Run the default validation workflow for first-party engine changes.

If this session is Failure promotion (`/promote`), do that RED then GREEN cycle first; it does not replace these steps.

## Steps

1. Read [docs/agents/common-tasks.md](docs/agents/common-tasks.md) for task-specific extras.
2. Build. If no existing test name under `engine/src/tests/` contains a distinctive stem from the edited basename, this `engine_editor` build is the Completion evidence:
   ```powershell
   cmake --build build/vs2026-debug --config Debug --target engine_editor
   ```
3. If a test name contains a stem from the edited files, run it (compiling the `*_test` target is not a Test run):
   ```powershell
   ctest --test-dir build/vs2026-debug -C Debug -R "<stem>" --output-on-failure
   ```
   Prefer `ctest --output-on-failure` so the Agent environment can observe success. See [docs/agents/testing.md](docs/agents/testing.md).
4. If meshes, textures, or import pipeline changed: re-cook per [CONTENT_LAYOUT.md](CONTENT_LAYOUT.md) or run `asset_compiler --force`.

Merge CI (GitHub Actions Linux) is a merge-time gate, not this command and not Completion evidence. See [docs/agents/testing.md](docs/agents/testing.md#merge-ci).

## Report

- Build succeeded or failed (include relevant errors).
- Which Test runs were observed in Shell (command + result). Chat claims do not count.
- Any follow-up needed (re-cook, Slint rebuild, etc.).
