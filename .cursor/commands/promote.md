# Promote escaped defect

<!-- BLUNDER_PROMOTION_ARMING -->

This session is **Failure promotion**. The Agent environment records arming from this command; do not claim you armed it in chat.

## Do this in Shell (chat is not evidence)

1. Add or change a first-party test under `engine/src/tests/` that fails on the escaped defect. Wire CMake if the target is new. See [docs/agents/testing.md](docs/agents/testing.md).
2. Run it so a **failing** Test run is observed (RED). `cmake --build … --target *_test` is a build, not a Test run.
   ```powershell
   ctest --test-dir build/vs2026-debug -C Debug -R "<test_name>" --output-on-failure
   ```
3. Fix the product code.
4. Run the **same test name** until it passes (GREEN).
5. Still produce Phase 1 Completion evidence for every first-party C++ edit in this session.

One RED then GREEN pair on the same test name is enough for Promotion evidence. A hook edit does not close promotion.

See [CONTEXT.md — Agent environment](CONTEXT.md#agent-environment-repository).
