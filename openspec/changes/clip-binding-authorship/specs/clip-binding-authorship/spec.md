## Purpose

Modern Clip Binding authorship on AnimationPlayer (asset assign + logical aliases, no pasted GUID) and marked Behaviour clip-name fields as weak logical-name dropdowns over the co-located player map.

## ADDED Requirements

### Requirement: Clip Binding row shows name and AnimationClip, not GUID
Each AnimationPlayer clip-map row SHALL present an editable logical name, the assigned AnimationClip identity (display name or equivalent), and Remove. The regular Inspector SHALL NOT show a GUID text field as the fill or edit surface for that row. The durable Asset Reference inside the binding remains the AnimationClip GUID in scene serialization.

#### Scenario: Inspector hides GUID on a bound row
- **WHEN** the author has a Clip Binding with a non-empty logical name and a valid AnimationClip reference
- **THEN** the AnimationPlayer Inspector row does not show an editable GUID field
- **AND** the scene still persists the AnimationClip GUID for that binding

### Requirement: Add clip creates a complete binding via picker
**Add clip** SHALL open an AnimationClip picker. On confirm, the engine SHALL append one Clip Binding whose Asset Reference is the chosen clip and whose logical name defaults to that clip’s registered stem when the author did not supply another name. Cancel SHALL add no row. Clips SHALL NOT appear in Add…. Empty name and empty reference draft rows SHALL NOT be created by Add clip.

#### Scenario: Add clip confirm appends stem-named binding
- **WHEN** the author clicks Add clip, picks AnimationClip stem `LOOP-chocomel-idle`, and confirms
- **THEN** the player map has a binding named `LOOP-chocomel-idle` referencing that clip
- **AND** no empty name/GUID draft row was added

#### Scenario: Add clip cancel leaves map unchanged
- **WHEN** the author clicks Add clip and cancels the picker
- **THEN** the clip map is unchanged

### Requirement: Location-sensitive Content Browser drop
Dropping an AnimationClip onto the empty clip list or the Add clip area SHALL append a new Clip Binding (logical name defaults to the clip stem). Dropping an AnimationClip onto an existing row SHALL retarget only that row’s AnimationClip Asset Reference and SHALL keep that row’s logical name. The per-row AnimationClip picker SHALL retarget that row the same way.

#### Scenario: Drop on empty list appends
- **WHEN** the player map is empty and the author drops AnimationClip `LOOP-chocomel-walk` onto the empty list or Add clip area
- **THEN** a new binding named `LOOP-chocomel-walk` referencing that clip is appended

#### Scenario: Drop on named row keeps alias
- **WHEN** a row named `idle` references clip A and the author drops clip B onto that row
- **THEN** the row remains named `idle` and references clip B

### Requirement: Logical names are unique aliases
Logical names on one AnimationPlayer SHALL be unique. An append or rename whose chosen or default name is already taken SHALL be rejected without silently overwriting another binding. Replacing a row’s AnimationClip SHALL NOT change its logical name. Two different logical names MAY reference the same AnimationClip.

#### Scenario: Append rejects duplicate stem
- **WHEN** the map already has `LOOP-chocomel-idle` and the author tries to append the same stem again via Add clip or drop-append
- **THEN** the map is unchanged (no overwrite)

#### Scenario: Rename rejects collision
- **WHEN** the map has `idle` and `walk` and the author renames `walk` to `idle`
- **THEN** the rename fails and both rows keep their prior names

#### Scenario: Two aliases one clip allowed
- **WHEN** the author creates bindings `idle` and `LOOP-chocomel-idle` that reference the same AnimationClip GUID
- **THEN** both bindings remain on the player

### Requirement: Dual-empty rows discarded on load
When loading a scene, Clip Binding rows whose logical name and AnimationClip reference are both empty SHALL be discarded. Rows that have a name without a clip or a clip without a name SHALL be kept and shown as invalid for repair.

#### Scenario: Load strips empty drafts
- **WHEN** a scene file contains a clip map entry with empty name and empty GUID
- **THEN** after load that entry is absent from the AnimationPlayer map

#### Scenario: Half-filled row kept invalid
- **WHEN** a scene file contains a binding with name `idle` and empty GUID
- **THEN** after load the row named `idle` remains and the Inspector marks it invalid

### Requirement: Marked Behaviour clip name is a weak logical-name dropdown
A Behaviour string member explicitly marked as a clip-name field SHALL be edited in the Inspector only by picking from the co-located AnimationPlayer’s current logical names (including an empty choice). Unmarked string members SHALL remain free text. Dropping an AnimationClip onto a Behaviour clip-name field SHALL NOT create or retarget Clip Bindings. Renaming or removing a Clip Binding SHALL NOT rewrite Behaviour property values; a stored name that no longer resolves SHALL remain stored and show as invalid until the author re-picks. When the Object has no AnimationPlayer or the map is empty, the dropdown SHALL offer only the empty choice (clear allowed; no free-typed names). Play SHALL treat an empty clip-name value as not playing that role.

#### Scenario: IdleClip picks from player map
- **WHEN** the player map has `LOOP-chocomel-idle` and `LOOP-chocomel-walk` and the author opens a marked `IdleClip` field
- **THEN** the dropdown lists those logical names (and empty)
- **AND** choosing `LOOP-chocomel-idle` stores that string in the Behaviour property bag

#### Scenario: No cascade on rename
- **WHEN** `IdleClip` is `idle` and the author renames the Clip Binding from `idle` to `rest`
- **THEN** `IdleClip` remains the string `idle` and shows as invalid

#### Scenario: Empty map allows only empty choice
- **WHEN** the Object has an AnimationPlayer with an empty clip map and a marked clip-name field
- **THEN** the author can clear the field to empty but cannot type a free-form clip name
