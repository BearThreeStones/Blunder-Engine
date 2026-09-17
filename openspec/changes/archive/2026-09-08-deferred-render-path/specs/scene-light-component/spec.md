## ADDED Requirements

### Requirement: Deferred lighting uses linking and the evaluation cap per receiver
When the editor viewport uses the Deferred Render Path, lighting SHALL apply Light linking and the Light evaluation cap per G-buffer receiver (the MeshRenderer that wrote that pixel). Per-pixel evaluation SHALL take at most 8 Light enabled lights that affect that MeshRenderer, in stable EntityId order. This SHALL NOT raise the Light evaluation cap. A global first-8 that ignores Light linking SHALL NOT be used.

#### Scenario: Non-empty linking excludes others on Deferred
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a Point Light’s receiver list contains only entity A and entity B is another MeshRenderer
- **THEN** A’s pixels are affected by that Point Light and B’s pixels are not

#### Scenario: Ninth affecting light is dropped on Deferred
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** nine Light enabled Point Lights all have empty linking lists
- **THEN** a MeshRenderer’s pixels are shaded by the first 8 in stable EntityId order and not the ninth

### Requirement: Deferred light list upload cap
The Deferred Render Path lighting pass SHALL upload at most 32 Light enabled candidates in stable EntityId order (Active in Hierarchy, not a contribution no-op). Lights beyond 32 SHALL be dropped from that list before per-receiver filtering. That 32 SHALL NOT be the Light evaluation cap.

#### Scenario: Thirty-third light does not enter the list
- **WHEN** 33 Light enabled Point Lights all have empty linking lists
- **THEN** the Deferred light list contains the first 32 in stable EntityId order
- **AND** the 33rd light is not in that list
- **AND** per-receiver evaluation still applies at most 8 from that list
