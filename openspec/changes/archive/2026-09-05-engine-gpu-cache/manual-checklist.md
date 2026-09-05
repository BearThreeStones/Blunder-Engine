# Manual checklist — engine-gpu-cache

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the Test Project (`E:\Blunder Projects\Test`). Story 1 needs two editor starts after emptying the Engine GPU cache (`%LOCALAPPDATA%\Blunder\gpu-cache` on Windows, unless `BLUNDER_GPU_CACHE_DIR` is set). Story 3: corrupt a file in that directory, then start. Story 4: Play from the editor (separate Player process).

| # | User story | Pass |
|---|------------|------|
| 1 | After emptying the user-level Engine GPU cache, the first editor open draws the viewport correctly; a second open of the same editor on the same GPU skips full Slang compile for unchanged Engine shaders, and the picture still matches. | |
| 2 | After editing `pbr.slang` and starting again, that shader’s bytecode cache misses and the viewport is still correct. | |
| 3 | After corrupting a cache file and starting again, the editor still starts and the viewport is still correct (treat as miss, rebuild, do not FATAL). | |
| 4 | Enter Play from the editor: Player is another process, still uses the same user-level cache directory, and Play looks correct. | |
