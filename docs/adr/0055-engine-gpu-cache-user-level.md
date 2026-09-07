# Engine GPU cache is user-level, not Project `.blunder/`

Shader bytecode cache (SPIR-V plus the Shader resource layout from that compile) and Pipeline cache live in a user-level directory outside every Project. Editor and Player on that machine share it. Rejected: Project `.blunder/` (Engine shaders are not Project content; every Project would recompile), `.blunder/cooked/` (Cooked cache is Final Assets), and the build output directory (a clean rebuild would drop the hit path).

Windows uses `%LOCALAPPDATA%/Blunder/gpu-cache/` so the blobs do not roam with `%APPDATA%` Project List config. A corrupt blob is discarded and rebuilt; it does not fail process start.
