# Product Play is Standalone plus preview and patch, not PIE

Blunder’s product **Play Mode** is one **Player** process (Standalone): two worlds, two processes. The editor stays in **Edit Mode**. Authors reach the running world with **Play Reload** and **Play authorship patch**; they see Tick-driven motion in the editor viewport with **Play pose preview** (poses on the Play control channel, no Live write, no Document History). Rejected as product Play: in-process PIE (a second duplicated world in the editor process), Unity-style in-place Tick on the authorship SceneInstance, and shipping PIE and Standalone as two first-class Play stacks.

Ship order: Play Reload, then Play authorship patch, then Play pose preview. ALC and Player Asset hot-reload stay later on the same Player process. Decision records for the bridges: [ADR 0047](0047-play-reload.md), [ADR 0048](0048-play-authorship-patch.md). Process boundary: [ADR 0014](0014-play-mode-separate-player-process.md).

## Considered Options

- **PIE + Standalone as equal Play modes** — rejected; Pause, Reload, Scripts, input, and tests would fork. Standalone already exists; seeing motion does not require a second Play implementation.
- **Unity in-place Play as the “preview” path** — rejected; Tick would dirty Live and Undo.
- **Write Play poses into Live / Keep Simulation Changes as the preview** — rejected; that is authorship write-back, not observation.
- **Play frame stills as the editor scene view** — rejected; a 16:9 Host still is not Editor Camera orbit around moving bodies.
