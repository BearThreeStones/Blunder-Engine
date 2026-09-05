# Manual checklist — hydrate-skeleton-on-instantiate

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run (story 1 blocked on idle loop; stories 2–4 walked 2026-09-02)

Windowed `engine_editor` on the Test Project (`E:\Blunder Projects\Test`), unless a row says otherwise.

| # | User story | Pass |
|---|------------|------|
| 1 | Quit the editor, reopen Test, open `chocomel_locomotion`, Play — Player window walks (idle at rest, stick walk), not a T-pose | |
| 2 | New editor session, open that scene — Inspector lists named bones without Add… Skeleton again | x |
| 3 | Open a cube (or other) entity with empty Skeleton and no AnimationPlayer and no AnimationTree — after load the Skeleton is still empty | x |
| 4 | GEO child still has an empty Skeleton after load; skinning still uses the parent’s hydrated bones | x |
