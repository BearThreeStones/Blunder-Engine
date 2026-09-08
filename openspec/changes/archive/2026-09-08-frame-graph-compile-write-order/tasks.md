## 1. Compile edges and InvalidPass

- [x] 1.1 Add `FrameGraphCompileReason::InvalidPass`. GraphBuilder `read` / `write` / `markSink` on a never-added Pass handle set a sticky flag (do not invent a Pass, do not throw).
- [x] 1.2 Replace RAW-only writer×reader edges with per-resource Setup-order walk: last Write → Read (RAW), last Write → Write (WAW), Reads since last Write → Write (WAR). Skip same-Pass edges. Remove the WAW/WAR gstack-shortcut comment.
- [x] 1.3 Compile failure order: `InvalidPass`, then `DanglingAccess`, then `NoSink`, then live `Cycle`. Success path unchanged: DCE from Sinks, Kahn, Resource lifetimes.

## 2. Tests

- [x] 2.1 Extend `frame_graph_test`: ColorAttachment Clear→Forward Sink (order + lifetime); Write→Read→Write Sink (reader before TAA); two Reads then Sink Write (both live, before writer, no required order between readers); same-Pass Write then Read or double Write is not `Cycle`.
- [x] 2.2 Invalid Pass on `read` / `write` / `markSink` → `InvalidPass` and empty live order. Keep dangling Resource as `DanglingAccess`. Add a stacked-error case that asserts the priority. Keep slice 1 cases (RAW two-pass, DCE, External, Buffer, live cycle, unreachable cycle, NoSink).
- [x] 2.3 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. Docs

- [x] 3.1 Extend [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md): Setup-order RAW/WAW/WAR (not Resource version); `InvalidPass` vs silent drop. Do not add ADR 0062. Keep CONTEXT terms already locked in Grill.
