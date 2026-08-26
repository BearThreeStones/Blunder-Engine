## 1. Launch parse

- [x] 1.1 Extend `editor_launch` for `--mcp`, `--scene`, CLI verb, imply Headless, adapter `--project-root` required (no Debug compiled-root fallback). Bare `--headless` unchanged
- [x] 1.2 Tests in `editor_launch_test`: MCP/CLI imply Headless; Debug `--mcp` without `--project-root` fails `launch.project_root_required`; `--scene` recorded; windowed+adapter rejected

## 2. Shared dispatch

- [x] 2.1 Machine adapter dispatch: Query / Op / Diagnose / Capture / Play session verbs / Save / CLI play-frame episode. Fail-closed codes from specs. CLI Op without `--save` does not call Op
- [x] 2.2 PNG encode of RGBA via `stb_image_write`; CLI `--out` overwrite; MCP ImageContent helper (base64 PNG)
- [x] 2.3 CLI result JSON on stdout (ok, failure_code, issues). Tests without Vulkan: request failures, Diagnose exit-0 with Issues, Op `--save` gate

## 3. Editor wiring

- [x] 3.1 `engine_editor`: adapter launches skip startup-scene default; `--scene` opens Live. CLI runs one verb then quit. MCP stdio JSON-RPC (initialize, tools/list, tools/call) interleaved with Headless tick
- [x] 3.2 CLI play-frame: `play` + optional Pause/step + `waitForPlayFrame` + `stop`. MCP play-frame fails when Stopped
- [x] 3.3 Tests: dispatch + launch (no full GPU boot required). Document GPU smoke in `docs/agents/testing.md` if needed

## 4. Docs

- [x] 4.1 CONTEXT / ADR 0044 already match grilling; fix any drift after implementation
