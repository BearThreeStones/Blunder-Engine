# Play Reload reinstantiates the captured On-disk Play entry

A running **Play session** may replace the Play Process gameplay world without ending that process: **Play Reload** instantiates the session’s captured On-disk **Play entry scene** (GUID/path frozen at spawn). The **Live document** is not the source; sealed Live Commands are **Play authorship patch** ([ADR 0048](0048-play-authorship-patch.md)), not Reload, and Reload does not replay them. A dedicated Reload control on **Play controls** starts it; Save does not; Play-while-Playing remains Stop then a new Play Process. Reload skips **Play Scripts build** and ALC (new C# still needs Stop then Play), preserves Playing versus **Play Pause** (Ready on remount still runs), does not Console-clear, and on preflight or instantiate failure leaves the current world in place. Windowed dirty Live uses the same **Play dirty prompt** as Play start; Headless uses last saved.

Rejected: dumping the Live `SceneInstance` across processes; treating Reload as Stop then Play; auto-Reload on Save; retargeting Reload to whichever scene is active now; building or replacing Scripts as part of Reload; tearing down the Player because Reload failed.

## Considered Options

- **Live document as Reload source** — rejected; **Play entry scene** is already the saved asset, and cross-process dump was rejected in [ADR 0014](0014-play-mode-separate-player-process.md).
- **Save while Playing Reloads** — rejected; Save is authorship persist; binding it to world-replace drops gameplay state by accident.
- **Play-while-Playing becomes Reload** — rejected; that path must keep a full process restart so Scripts can rebuild and load.
- **Replay Live Commands after Reload** — rejected; that would make Reload a Live dump by another door ([ADR 0048](0048-play-authorship-patch.md) stays the Live path).
