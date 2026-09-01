## Why

The Animation Window dropdown looks like the clip you are previewing, but it is only the Fire target. Left **Play** Clip Plays the scene default instead, so Chocomel can show idle in the combo while walk plays in the viewport and on the ruler. Authors need **Preview clip** to be the clip Play, bind, dropdown change, and Fire all address.

## User stories

1. I select Chocomel. **Preview clip** defaults to walk. The window Clip Plays walk and stays at t=0. Viewport, ruler, and Clip anatomy all show walk. The document stays clean.
2. I press Play. Walk advances. While Playing I change Preview clip to idle. Idle hard-cuts at t=0. Viewport, ruler, and anatomy follow idle.
3. I Pause or Stop, then change Preview clip to another clip. That clip Clip Plays at t=0 and does not start transport. Stop does not clear Clip Play override. The ruler stays on that clip at frame 0.
4. Preview clip is idle. I press window Fire. Idle occupies the Fire slot and the ruler follows the insert. While Fire occupies I change Preview clip to walk. Fire keeps playing; the Clip Play base underneath becomes walk. When Fire ends the ruler shows walk.
5. I select a different Object. The previous tree's Clip Play override is cleared. If the new Object has an AnimationTree, Preview clip is its default clip and Clip Plays at t=0 without playing. If it has no Tree, the panel stays open and disabled, with no Clip Play or anatomy.

## What Changes

- Rename the session dropdown to **Preview clip** (not Fire target)
- **Play** Clip Plays Preview clip (activate inactive tree first), then advances
- Changing Preview clip Clip Plays the new name immediately (hard cut, clock 0) whether Playing, Paused, or Stopped; Playing continues; Paused and Stopped do not start transport
- Binding a Tree sets Preview clip to the scene default clip, activates if needed, Clip Plays at 0, does not start transport
- Window **Stop** seeks 0, clears Fire, Ends CINE, leaves the tree active; it does **not** clear Clip Play override
- Hierarchy unbind still clears the previous tree's Clip Play override
- Window **Fire** still inserts Preview clip on the Fire slot; while Fire occupies, changing Preview clip retargets only the Clip Play base; ruler stays on the insert until Fire ends
- Preview clip stays session-only (not Document History)
- Remove “v1 SHALL NOT provide a Clip Play control” and “Fire target SHALL NOT Clip Play”

**Out of scope:** a second Clip Play dropdown; Travel / BlendSpace / Add2 in this window; keyframe authorship; Play Mode; serializing Clip Play override

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `animation-window`: Preview clip is the session Clip Binding name for Play, bind, dropdown change, and Fire; Stop no longer clears Clip Play override; bind Clip Plays the default at t=0

## Impact

- `AnimationPreviewController`: Play uses Preview clip; `setFireTarget` (or renamed setter) Clip Plays without starting transport; bind Clip Plays default; `stop()` does not `clearClipPlay`; `haltBoundSession` / unbind still clears the previous tree's override
- `animation_window_panel.slint` / Slint bind: combo remains one list; no second dropdown
- Tests: Play vs Preview clip; dropdown change in Playing / Paused / Stopped; Stop keeps override; bind default Clip Play; Fire occupying + Preview clip change; rebind clears previous override
- Docs: CONTEXT **Preview clip** / **Animation Window** / **Clip Play** already locked in Grill
- Human acceptance: walk the five User stories in the windowed editor
