## ADDED Requirements

### Requirement: Dock tabs use Editor Theme pill chrome
The integrated dock chrome row SHALL use Editor Theme Application Bar ground and pill tabs (idle unfilled, active filled) with Editor corner radius on the tab. Close and pin glyphs SHALL use theme icon gray (`#B3BBC4`) and existing hover brighten behavior. Tab-drag, click-without-drag, chrome-blank float move, and chrome height hit-testing SHALL stay as specified by the other dock-browser-tab-chrome requirements.

#### Scenario: Active dock tab is a filled pill
- **WHEN** a docked container shows an active tab
- **THEN** that tab is a filled pill on Application Bar ground
- **AND** dragging it still starts the existing tab-drag path after the tab-drag threshold
