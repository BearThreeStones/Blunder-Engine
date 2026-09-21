## ADDED Requirements

### Requirement: Player color target uses the Deferred Render Path
A windowed or Headless Player SHALL produce the Play-rule camera color target with the Deferred Render Path (G-buffer then clustered lighting). Blend-transparent SHALL remain Forward after lighting. Editor Overlays SHALL stay off. Image-based VRS MAY attach to that lighting pass when the device has `VK_KHR_fragment_shading_rate` and `BLUNDER_EDITOR_VRS` is not force-off. Headless Player SHALL NOT require that extension to start or to send a Play frame. The Player SHALL NOT draw the editor rate-mask overlay.

#### Scenario: Windowed Play is deferred
- **WHEN** a windowed Player presents a frame of the Play-rule camera
- **THEN** opaque shading SHALL be G-buffer then clustered lighting
- **AND** Editor Overlays SHALL not draw

#### Scenario: Headless Play frame without VRS extension
- **WHEN** a Headless Player is running
- **AND** the device lacks `VK_KHR_fragment_shading_rate`
- **THEN** a Play frame SHALL still be sent on the Play control channel
- **AND** no Player OS window exists
