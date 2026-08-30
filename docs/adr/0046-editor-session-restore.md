# Editor Session restore is Project-local cache, not Save

A windowed **Editor Session** remembers **dock layout** (splits, tabs, floating panels, auto-hide) and the last Live **Scene Asset** by **Asset Reference (GUID)** in that Project’s `.blunder/` cache. The next windowed session on the same Project restores them. That is **Editor Session restore**, not Application Bar **Save**, not a committed Project File field, and not adapter Live. `--scene` still wins for which scene opens; dock layout still restores. Missing Scene Asset GUID, Headless, CLI, and MCP do not use this restore.

Rejected: writing this into `project.blunder` or the user **Project List**; treating virtual path as identity after Rename / reparent; letting `BLUNDER_STARTUP_SCENE` beat a remembered GUID; skipping dock restore because `--scene` was set; reopen-last-Project as the editor’s no-arg default ([ADR 0010](0010-debug-may-skip-project-manager.md)); persisting OS window geometry, Viewport camera, Browser folder, or selection in this slice.

## Considered Options

- **Application Bar Save also writes layout / last scene** — rejected; Save already means the Live document to a Scene Asset. Mixing chrome memory into document persist confuses dirty prompts and History.
- **Store in `project.blunder`** — rejected; the Project File is identity (`name`). Layout would enter git and fight across machines.
- **Store beside the Project List (user roaming)** — rejected; restore is “this Project folder on this machine.” Path-keyed user files break when the root moves; it also rhymes with reopen-last-Project.
- **Remember virtual path only** — rejected; Open Scene follow already keeps the document across Rename / reparent. GUID survives those while the editor is closed; a missing GUID falls through to env then the compiled default startup scene.
- **`BLUNDER_STARTUP_SCENE` always wins over restore** — rejected for windowed GUI; env remains the fallback when there is no remembered GUID (or it does not resolve). `--scene` remains the explicit Live override.
- **Reset layout menu in this slice** — rejected; inject missing panel kinds at their default home. Unreadable restore falls back to the default dock layout.

## Consequences

Two windowed sessions on one Project last-write the restore record (no merge, no lock). New dock panel kinds inject into a remembered layout; they do not wipe it. Glossary: `CONTEXT.md` (**Editor Session restore**).
