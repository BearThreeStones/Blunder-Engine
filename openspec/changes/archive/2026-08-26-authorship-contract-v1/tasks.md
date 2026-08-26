## 1. Issue types and Play-rule Diagnose

- [x] 1.1 Add Issue / Request failure / Issue severity types (stable string codes from design D2)
- [x] 1.2 Add Play-rule Diagnose functions over a Scene + Project scripts dirtiness (no compile, no Cook). Missing DLL → `scripts.missing_output` only; newer sources with DLL → `scripts.dirty` only; no Camera → `play.missing_camera`
- [x] 1.3 Extend `play_preflight_test` (or a sibling test) for those Issue codes and for Diagnose not invoking a build hook
- [x] 1.4 Map `runPlayCameraGate` to Diagnose Issues; keep a human explanation string for existing log/UI

## 2. Authorship System (Live Query / Op)

- [x] 2.1 Add Authorship System under `engine/src/runtime/function/editor/`; wire CMake + `RuntimeGlobalContext`. Create only for Editor host; Player MUST NOT mount it
- [x] 2.2 Live Query: name list + get entity (name, parent name, local TRS). Skip tombstones and empty names. Unknown / tombstoned / empty → `address.unknown`. No open scene → `subject.no_live_document`
- [x] 2.3 Transform Op: apply full local TRS then `push(makeSetEntityTransformCommand)`. Op against On-disk → `subject.live_required`. Success = one Document History Command
- [x] 2.4 On-disk Query / Diagnose helpers: Scene Asset virtual path + Project root; unreadable scene → `subject.scene_unreadable`. Callable without Authorship System
- [x] 2.5 Add `authorship_contract_test`: Query list/get, tombstone unknown, transform Op undo, Diagnose missing camera, Op on disk fails, Player-host does not create the System (or equivalent host_mode check)

## 3. Play start consumes Issues

- [x] 3.1 `startPlaySession` Camera gate uses Play-rule Diagnose on the scene that will load; Error `play.missing_camera` blocks spawn and sets last error from the Issue
- [x] 3.2 `PlaySessionController` Scripts build failure reports Error Issue `scripts.build_failed` (not Diagnose). Dirty Diagnose Warning does not skip the build
- [x] 3.3 Extend `play_preflight_test` / `play_session_controller_test` so failed build and missing Camera assert Issue codes

## 4. Docs and validation

- [x] 4.1 Confirm CONTEXT Authorship terms and ADR 0041 match the shipped API (no extra glossary churn)
- [x] 4.2 Document `authorship_contract_test` in `docs/agents/testing.md` (ctest regex)
- [x] 4.3 Build `authorship_contract_test` and `play_preflight_test`; run them
