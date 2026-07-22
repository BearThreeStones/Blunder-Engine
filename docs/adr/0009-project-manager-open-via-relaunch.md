# Project Manager opens Projects by spawning engine_editor

**Project Manager** (`project_manager.exe`) and **Editor Session** (`engine_editor.exe`) are separate executables that ship in the same output directory. For v1, **Project Open** always spawns `engine_editor` with `--project-root` rather than hot-swapping `FileSystem` / Vulkan / Slint state in-process. Rejected for v1: live re-init onto a new root (complex teardown); keeping Manager as a mode of `engine_editor` (harder packaging and day-to-day entry); a full Unity Hub–style multi-version product (out of MVP scope).
