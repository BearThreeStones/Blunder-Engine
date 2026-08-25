## ADDED Requirements

### Requirement: Hierarchy Camera icon does not drive Camera Preview
Alt+left-pointer down on a Hierarchy Camera Unique icon SHALL NOT show, hide, or change the Camera Preview target. Camera Preview SHALL continue to follow its existing selection-based visibility rules.

#### Scenario: Alt+LMB Camera icon leaves Camera Preview rule unchanged
- **WHEN** the author Alt+LMBs the Hierarchy Camera icon
- **THEN** Camera Preview does not open or retarget as a result of that gesture
- **AND** Attachment property preview for the Camera Component may open per that capability
