# Play authorship patch applies sealed Live Commands by entity name

While a **Play session** runs, Document-History-sealed **Editor Commands** on the Live document that **is** this session’s **Play entry scene** may be applied onto the Play Process world as **Play authorship patch** — without Save and without **Play Reload** ([ADR 0047](0047-play-reload.md)). The key is **Authorship Address** (scene-unique entity name), not `EntityId` or `ObjectId`. Patches ride the existing **Play control channel**. They write at Command seal (Playing or Paused); later Tick may overwrite; there is no authorship lock. Undo, Redo, and History Jump of a v1-patchable Command also patch; a missed Player entity is a Warning-grade Issue and does not roll back History or **Error Pause**. v1 covers authored data on entities that already exist in both worlds (Local Transform, Unique, MeshRenderer, Behaviour bags, SkeletonModifiers, animation-host fields). Spawn, delete, rename, reparent, Global Commands, ALC, and Player Asset hot-reload are not this.

Rejected: dumping the Live `SceneInstance`; streaming uncommitted gizmo samples; Pause-only patches; locking patched properties against Tick; patching a Live document that is not the Play entry; a second Play-session socket for patches.

## Considered Options

- **On-disk deltas only (must Save first)** — rejected; that is a partial Reload, not live authorship.
- **Uncommitted pointer samples** — rejected; fights **Transform field commit** / Command coalescing.
- **Pause-only apply** — rejected; tuning values scripts read each Tick would require freezing the game.
- **Authorship lock until Reload** — rejected; Tick and patches deadlock without per-field dirty flags.
- **All Document Commands including Spawn/delete/rename** — deferred; v1 is retune, not live level edit. Structural change still uses Reload.
