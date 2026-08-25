## Purpose

Lets authors Alt+LMB a Hierarchy row icon to edit that one attachment in a floating Inspector-like card, with pin and Document History, without using Camera Preview or a second Inspector dock.

## ADDED Requirements

### Requirement: Alt+LMB opens attachment property preview
Alt+left-pointer down on a Hierarchy row icon SHALL select that row’s entity and open an **Attachment property preview** card for that icon’s attachment (Local Transform, MeshRenderer, Unique, Behaviour, or SkeletonModifier). The card SHALL show the same authorable fields as that attachment’s Inspector section. Plain left-pointer down on an icon SHALL NOT open a preview. Alt+LMB on the entity name, Hierarchy Line gutter, or expand chevron SHALL NOT open a preview. Preview SHALL NOT play clips and SHALL NOT be **Edit animation preview**.

#### Scenario: Alt+LMB Transform icon
- **WHEN** the author Alt+LMBs the Local Transform icon on an entity row
- **THEN** a preview card shows that entity’s Local Transform fields
- **AND** that entity is selected

#### Scenario: LMB icon does not preview
- **WHEN** the author LMBs a Hierarchy row icon without Alt
- **THEN** that entity is selected
- **AND** no attachment property preview opens from that press

#### Scenario: Alt+LMB name does not preview
- **WHEN** the author Alt+LMBs the entity name
- **THEN** no attachment property preview opens from that press

#### Scenario: Behaviour index
- **WHEN** the author Alt+LMBs the second Behaviour icon on a row with two Behaviours
- **THEN** the card shows the second Behaviour’s Inspector fields

### Requirement: Camera icon is not Camera Preview
Alt+LMB on the Hierarchy Camera Unique icon SHALL open **Attachment property preview** for the Camera Component. That gesture SHALL NOT open, close, or retarget **Camera Preview**.

#### Scenario: Camera icon Alt+LMB
- **WHEN** the author Alt+LMBs the Camera icon on a Camera entity
- **THEN** the Camera Component property card is shown
- **AND** Camera Preview visibility still follows its existing selection rule, not this gesture

### Requirement: Pin locks the card
Each preview card SHALL have a pin that is not the dock auto-hide pin. A pinned card SHALL remain showing that entity+attachment when Hierarchy selection changes. Unpin or close SHALL dismiss that card. An unpinned card SHALL close when selection changes to another entity or when the author Alt+LMBs a different icon. Several pinned cards MAY be open at once.

#### Scenario: Pinned survives selection change
- **WHEN** a Transform preview is pinned on entity A and the author selects entity B
- **THEN** the Transform card for A stays open

#### Scenario: Unpinned closes on selection change
- **WHEN** an unpinned Light preview is open on entity A and the author selects entity B
- **THEN** that Light card closes

### Requirement: Same-icon Alt+LMB
Alt+LMB on the icon for an already open unpinned card SHALL close that card and SHALL NOT open a second card for the same attachment. Alt+LMB on the icon for an already open pinned card SHALL raise that card, SHALL NOT close it, SHALL NOT unpin it, and SHALL NOT open a second card for the same attachment.

#### Scenario: Unpinned toggle close
- **WHEN** an unpinned Skeleton preview is open and the author Alt+LMBs that Skeleton icon again
- **THEN** that card closes

#### Scenario: Pinned raise
- **WHEN** a pinned AnimationTree preview is open and the author Alt+LMBs that AnimationTree icon again
- **THEN** that same card is raised
- **AND** a second AnimationTree card is not created

### Requirement: Preview edits use Document History
Field edits on an attachment property preview card SHALL push **Document History** Commands, including when the card is pinned and its entity is not the current selection. Those edits SHALL NOT push Global History. When an attachment property preview card has input focus, keyboard Undo/Redo SHALL target Document History, not Global History, unless an exclusive text-undo context claims the shortcut.

#### Scenario: Pinned card edit
- **WHEN** a pinned Transform card for entity A is open, entity B is selected, and the author commits a Position change on the card
- **THEN** Document History can undo that Transform change

#### Scenario: Preview focus undoes Document History
- **WHEN** Document History can undo and an attachment property preview card has input focus
- **AND** the user presses Ctrl+Z (and text-undo does not claim the shortcut)
- **THEN** the last Document History Command is undone
- **AND** Global History is unchanged

### Requirement: Card lifetime
If the card’s entity is deleted or that attachment is Removed, the card SHALL close. Pin SHALL NOT keep a missing attachment. Undo that restores the entity or attachment SHALL NOT reopen the card. Closing or switching the scene document SHALL close every preview card for that document; reopening the scene SHALL NOT restore pins. Entering Play Mode SHALL NOT close preview cards.

#### Scenario: Remove attachment closes card
- **WHEN** a pinned Camera preview is open and the author Removes Camera from that entity
- **THEN** that card closes

#### Scenario: Undo does not restore card
- **WHEN** a preview card closed because its attachment was Removed and the author undoes that Remove
- **THEN** the preview card does not reopen by itself

#### Scenario: Open another scene
- **WHEN** preview cards are open and the author opens a different scene document
- **THEN** those cards are closed

#### Scenario: Enter Play
- **WHEN** preview cards are open and the author enters Play Mode
- **THEN** those cards stay open
