# Blunder editor — Nsight Systems presets (CLI)

Tested with **Nsight Systems 2026.3.1**. Uses `--command-file` (scheme 2): capture via CLI, analyze in **nsys-ui**.

## Quick start

From repo root (**Administrator PowerShell** — required for Vulkan trace on Windows):

> **Cursor Agent / non-interactive terminals cannot show UAC.** If the script exits immediately with
> “Administrator PowerShell required”, open **Terminal (Admin)** manually and run the command it prints.
> Interactive Windows Terminal inside Cursor may still prompt UAC; Agent background shells will not.

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
.\profiling\nsight\run-group.ps1 -Group A -Duration 15
```

**Do not** pass `-ResolveSymbols` expecting CLI PDB resolve — on Windows, `nsys profile --resolve-symbols=true` **hangs at 19%** and leaves a **0-byte** `.nsys-rep` (even after 60+ minutes). Use **fast capture** above, then resolve symbols in nsys-ui (below).

If a prior capture hung at ~19% (0-byte `.nsys-rep`), reset first (UAC prompt if not elevated):

```powershell
.\profiling\nsight\stop-and-reset.ps1 -Group A
.\profiling\nsight\run-group.ps1 -Group A -Duration 5
```

Without elevation, `nsys` fails with `Failed to register Vulkan extension JSON file` and no `.nsys-rep` is written. CPU sampling (`--sample=process-tree`) also needs admin on Windows.

The script auto-finds `nsys.exe` under any drive’s `Program Files\NVIDIA Corporation\Nsight Systems *\target-windows-x64\`.  
Override if needed:

```powershell
$env:BLUNDER_NSIGHT_NSYS = 'D:\Program Files\NVIDIA Corporation\Nsight Systems 2026.3.1\target-windows-x64\nsys.exe'
```

Open the report in nsys-ui (path printed at end of script), or **File → Open** in Nsight Systems.

`run-group.ps1` defaults to **fast capture** (`--resolve-symbols` off). Stale `.sqlite` from a prior export is removed before each run — keeping it with `--force-overwrite` on the `.nsys-rep` can hang export at ~19% and leave a **0-byte** report.

`run-group.ps1` always records PDB search paths (`editor/profile`, `editor/Debug`, `runtime/Debug`).  
`-ResolveSymbols` only prints **post-capture** nsys-ui steps; it does **not** pass CLI `--resolve-symbols` (broken on Windows).

## Symbols (for AI / SQLite with function names)

1. **Fast capture** (no CLI resolve): `.\profiling\nsight\run-group.ps1 -Group A -Duration 15`
2. Open `.nsys-rep` in **nsys-ui**
3. **Project → right-click report → Resolve Symbols…** (PDB folders below)
4. **File → Export → SQLite**, or `.\profiling\nsight\export-sqlite.ps1 -Group A`

**Never use** `nsys profile --resolve-symbols=true` on Windows — hangs at 19%, 0-byte report.

In **Resolve Symbols…** dialog:

1. Add PDB folders (profile capture uses `engine_editor_profile.pdb`):
   - `build\vs2026-debug\engine\src\editor\profile`
   - `build\vs2026-debug\engine\src\editor\Debug`
2. Optional: Microsoft symbol server `https://msdl.microsoft.com/download/symbols` for system DLLs.
3. Check **Symbol Resolution Logs** for PDB mismatch; rebuild editor and re-capture if needed.

PDB must match the exe (same build). Debug config required for your own code symbols.

## Frame duration shows only 2 CPU frames (99% NA)

This is **expected** for Blunder, not a broken capture.

Nsight **Frame duration** counts **Present boundaries** (DXGI / swapchain), not engine loop iterations:

| Layer | Presents? |
|-------|-----------|
| 3D viewport (headless Vulkan offscreen) | **No** `vkQueuePresentKHR` |
| Slint / Skia (HWND) | **Yes** — only path Nsight can mark as a “frame” |

The editor **intentionally skips** most Skia presents:

- `shouldPresentSkiaFrame()` is true only when `m_viewport_frame_ready` (new viewport upload scheduled).
- Tiered pacing: **interactive** (~33 ms request floor, default) vs **idle** (~100 ms);
  `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS` (default 50) still caps Skia present while updating.
- `endFrame()` throttles Skia composite using the active tier when the viewport is updating.
- Resize / defer paths skip present (`shouldDeferHeavyFrameWork`, `m_present_suppress_frames`).

So **99% NA** means: CPU was busy, but almost no Present events occurred in that interval. The red **122 ms** block is usually the **first full Skia layout composite** after startup.

Verified on a 12 s idle run: **240** engine ticks vs **3** Skia presents (~97% skipped via `shouldPresentSkiaFrame()` / viewport-ready gate).

**What to use instead in nsys-ui:**

- **CPU thread rows** + sampling tooltips (hotspots: readback, Slint, SDL pump).
- **Vulkan** row for GPU queue work (3D passes).
- Do **not** use Frame duration row as engine FPS or render-tick count.

### Viewport pacing validation (tiered)

Compare **Vulkan → `vkQueueSubmit`** counts (not Frame duration):

| Scenario | Group A, 15 s | Pass |
|----------|---------------|------|
| Orbit / pan in viewport | Interactive tier | **≥ 18/s** (target 20–30/s) |
| Static viewport (no input) | Idle tier | **≤ 12/s** (target 5–10/s) |

Tune with `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS`, `BLUNDER_EDITOR_VIEWPORT_IDLE_MS`,
`BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_HOLD_MS`, and `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS`.

`run-group.ps1` builds to `build/.../editor/profile/engine_editor_profile.exe` when the runtime lib is newer (avoids a locked `Debug/engine_editor.exe` blocking relink). Pass `-DBLUNDER_EDITOR_PROFILE_EXE=ON` manually for a normal CMake configure.

## Groups

| Group | File | `BLUNDER_*` |
|-------|------|-------------|
| A | `A_baseline.txt` | (none) — Debug baseline |
| B | `B_zerocopy.txt` | `BLUNDER_VK_VALIDATION=0`, `BLUNDER_VIEWPORT_ZERO_COPY=1` |
| C | `C_no_partial.txt` | `BLUNDER_SLINT_PARTIAL=0` |
| D | `D_release_baseline.txt` | (none) — use `-Config Release` in script |

Shared trace options live in `common.txt` (Vulkan trace, CPU process-tree sampling, GPU metrics).

## Manual CLI

```powershell
cd E:\Dev\Blunder-Engine
nsys profile `
  --command-file=profiling\nsight\common.txt `
  --command-file=profiling\nsight\B_zerocopy.txt `
  --duration=15 `
  build\vs2026-debug\engine\src\editor\Debug\engine_editor.exe
```

Nsight 2026.x does **not** support nesting `--command-file` inside another command file; pass `common.txt` and the group file as two CLI flags (as `run-group.ps1` does).

## GUI mapping (2026.3.1 nsys-ui)

If you prefer clicking **Start** in the GUI instead of CLI:

| Command file | nsys-ui panel |
|--------------|---------------|
| `--env-var=...` | Target → **Environment variables** → Add |
| `--trace=vulkan` | **Trace** → Vulkan |
| `--sample=process-tree` | **CPU sampling** → process-tree (replaces legacy `osrt` trace in 2026.x) |
| `--gpu-metrics-devices=all` | **GPU metrics** → all devices (requires **admin**; omitted by default) |
| `--inherit-environment=true` | Environment → **Inherit base environment** |
| exe path | **Command line with arguments** → `engine_editor.exe` only |
| working dir | **Working directory** → repo root |

Do **not** put `nsys profile ...` in Command line — that field is for the target app only.

## Output

Reports are written under `profiling/nsight/reports/*.nsys-rep` (gitignored).

See also: [docs/agents/render-pipeline.md](../../docs/agents/render-pipeline.md), [.cursor/skills/viewport-perf/SKILL.md](../../.cursor/skills/viewport-perf/SKILL.md).
