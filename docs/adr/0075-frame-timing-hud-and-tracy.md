# Frame timing HUD and dock; Tracy is dual-write only

The 2026-10 DogWalk line needs a **basic profiler** (CPU / GPU frame time, main Passes, readable in the editor, packed GPU-driven `inst` / MeshBatch `batches` / surviving meshlet-indirect `cmds`, later light budgets and the install package) — not Insights. We decided two **Slint** product layers on an **engine Frame timing ring** and GPU timestamp queries: a **Frame timing HUD** on windowed editor Viewport **and** windowed Player, and an editor **Profiler dock** (frame strip / lanes / zone inspect). `cmds` is compact surviving meshlet indirect, not `vkCmd*` and not the CPU submit-list length. Tracy Client is optional **dual-write** to standalone `Tracy.exe` only. It is not a product layer and not the panel source. HUD and dock work with `TRACY_ENABLE` undefined. Dev Client uses `TRACY_ON_DEMAND` + localhost. CI and shipping do not define `TRACY_ENABLE`. `FrameMark` is `tickOneFrame`, not Present. GPU zones are named Frame graph Passes plus a few internals, never per-draw. Domain: [CONTEXT.md — Frame timing HUD](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Tracy window as Layer 1 / Layer 2** — rejected. Human locked: “editor can see” is in-process Slint; Tracy.exe is optional deep dive.
- **Embed Tracy View / Server (ImGui), or egui / Qt / WebView as a Slint component** — rejected. Slint is the only toolkit. Tracy embedding uses a global allocator and fights Skia. `TRACY_NO_ROOT_WINDOW` is ImGui-in-ImGui.
- **HUD/dock read Tracy buffers or `.tracy`** — rejected. Client has no `getLastFrames()`. Shipping must not link Client.
- **Buy a timeline (puffin, Optick, easy_profiler, microprofile, Remotery, `slint-ui-system`)** — rejected. None is Slint + C++ + Vulkan GPU zones. Switching capture would drop the Tracy Vulkan path and force a second toolkit.
- **Title-bar FPS as the Player readout** — rejected. Grill: same Slint HUD as the editor; title MAY remain, not the only number.
- **Keep windowed Player on SDL_Renderer blit** — rejected. Grill default: HUD-only Slint root + shared Vulkan device Present. Headless stays windowless.
- **Always-on Tracy Client** — rejected. Unconnected events grow to GB. `TRACY_ON_DEMAND` is required when `TRACY_ENABLE` is defined. `TRACY_ENABLE=0` is not off.
- **Per-draw / per-instance / per-Spot GPU zones** — rejected. 5000 / 138 use count plots + lighting/scene GPU ms.
- **FrameMark at Present** — rejected. Nsight: idle almost never Presents; engine 3D is offscreen.
- **Product settings UI / persist HUD or dock** — rejected. ADR 0062. Session toggle, default off, F3.
- **Convert Player to deferred in this knife** — rejected. That is VRS PR 37. Player timestamps `viewport.scene`.
- **Stack this planning git on PR 24, PR 36, or PR 37** — rejected. Branch from `main`.
