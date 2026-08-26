## ADDED Requirements

### Requirement: Headless uses the same observation
Headless Editor and Headless Player SHALL use Capture, Play step, and Play frame as specified for Host observation. They SHALL NOT introduce a second observation protocol. Capture SHALL NOT require Slint or an OS window.

#### Scenario: Headless Capture without a window
- **WHEN** Capture runs on a Headless Editor
- **THEN** the still is a 16:9 Scene still from the Scene still path
- **AND** no OS window or Slint composite is used

#### Scenario: Headless Play frame without a window
- **WHEN** a Headless Player is Paused and Play step then Play frame run
- **THEN** the editor receives a 16:9 Play frame of the Play Process world
- **AND** the frame is not an HWND scrape
