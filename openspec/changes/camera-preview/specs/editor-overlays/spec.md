## ADDED Requirements

### Requirement: Camera Preview is not an OverlaySystem draw

**Camera Preview** is authorship-only editor chrome and MUST NOT appear in the Player. It is NOT drawn by OverlaySystem into the main offscreen color target; it uses a separate Slint panel and a dedicated preview render target.

#### Scenario: Main viewport image excludes PiP pixels

- **WHEN** Camera Preview is visible
- **THEN** the main `viewport-image` does not contain the preview chrome or preview scene as baked pixels in its bottom-right corner
