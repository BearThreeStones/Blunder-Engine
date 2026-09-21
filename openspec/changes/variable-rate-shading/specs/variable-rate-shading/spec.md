# variable-rate-shading Specification

## Purpose

Lower fragment rate on clustered-deferred lighting with an image-based shading-rate attachment built from a previous-frame Sobel luminance edge (1×1 / 2×2). Optional `VK_KHR_fragment_shading_rate`. No Forward VRS path.

## ADDED Requirements

### Requirement: Clustered-deferred lighting uses an image-based shading rate
When a surface uses the Deferred Render Path and `VK_KHR_fragment_shading_rate` with attachment shading rate is enabled on the device, and `BLUNDER_EDITOR_VRS` is not a force-off (`0` / false), the lighting fullscreen triangle SHALL be recorded with a fragment shading rate **image attachment**. The attachment SHALL come from a path-owned rate image written by a previous-frame Sobel luminance-edge compute. The lighting Engine shader SHALL NOT change. Per-draw pipeline rate SHALL NOT be the source of that 1×1 / 2×2 pattern. Per-primitive shading rate SHALL NOT be written by task or mesh shaders for this change. G-buffer, shadows, blend-transparent, scene overlays, outline, pick, SSAO, and volumetric fog SHALL stay at fragment size 1×1.

#### Scenario: Lighting samples the previous rate image
- **WHEN** the device has fragment shading rate attachments
- **AND** `BLUNDER_EDITOR_VRS` is not force-off
- **AND** a deferred lighting pass records a frame after the first
- **THEN** that lighting triangle SHALL use the previous in-flight slot’s rate image as the shading-rate attachment
- **AND** `deferred_lighting.slang` SHALL be the same shader as without VRS

#### Scenario: First frame is 1×1
- **WHEN** a deferred lighting pass records its first frame on a target
- **THEN** the rate image texels used by that pass SHALL be 1×1 (cleared texel `0`)

#### Scenario: G-buffer is not rate-shaded
- **WHEN** VRS is enabled on a deferred surface
- **THEN** the G-buffer geometry pass SHALL not use that rate image
- **AND** G-buffer receiver ids SHALL not be written at 2×2

### Requirement: Rates are 1×1 and 2×2 from Sobel luminance edges
The Sobel compute SHALL read the just-written lighting color, convert RGB with Rec.709 luminance (`0.2126 R + 0.7152 G + 0.0722 B`), run a 3×3 Sobel, and write `G = dx² + dy²`. When `G > 0.1` the rate texel SHALL be `0` (1×1). Otherwise it SHALL be `1 << 2 | 1` (2×2). The compute SHALL run on the graphics queue after the lighting render pass ends and before that surface’s LOAD overlay or readback. Workgroup shape SHALL be 16×16 with an 18×18 halo. Encoding SHALL match the Vulkan fragment shading rate texel packing. 1×2, 2×1, and 4×4 SHALL NOT be produced this change. The rate image SHALL NOT be Bindless, an Asset, Cook output, or a Frame graph Transient.

#### Scenario: Edge stays 1×1
- **WHEN** a lighting-color neighbourhood has Sobel `G > 0.1`
- **THEN** the corresponding rate texel SHALL encode 1×1

#### Scenario: Flat region is 2×2
- **WHEN** a lighting-color neighbourhood has Sobel `G ≤ 0.1`
- **THEN** the corresponding rate texel SHALL encode 2×2

### Requirement: Missing extension or force-off is 1×1, never FATAL
Device create SHALL treat `VK_KHR_fragment_shading_rate` as optional, matching `VK_EXT_mesh_shader`. If the extension or attachment feature is missing, the engine SHALL log, SHALL skip rate images and Sobel, and SHALL keep today’s 1×1 lighting render pass. `BLUNDER_EDITOR_VRS=0` SHALL force that same skip even when the extension is present. Process start SHALL NOT FATAL because the extension is absent. There SHALL NOT be a software compute VRS path. Headless, CLI, MCP, and Linux Merge CI SHALL NOT require the extension. There SHALL NOT be a product settings UI for VRS.

#### Scenario: No extension still starts
- **WHEN** the selected device lacks `VK_KHR_fragment_shading_rate`
- **THEN** device create SHALL succeed
- **AND** deferred lighting SHALL run at 1×1
- **AND** the log SHALL record that VRS is off

#### Scenario: Env force-off
- **WHEN** the device has the extension
- **AND** `BLUNDER_EDITOR_VRS` is `0`
- **THEN** deferred lighting SHALL run at 1×1
- **AND** Sobel SHALL not dispatch

### Requirement: Lighting render pass stays SECONDARY
When VRS is enabled, only the deferred lighting Vulkan render pass SHALL be created with RenderPass2 and `VkFragmentShadingRateAttachmentInfoKHR`. Contents SHALL remain `VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS`. The lighting SECONDARY SHALL set combiners so pipeline rate is 1×1 and the attachment replaces. The engine SHALL NOT migrate to dynamic rendering for this change. The engine SHALL NOT change lighting to INLINE to attach the rate image.

#### Scenario: Secondary lighting still executes
- **WHEN** VRS is enabled and the editor Viewport records lighting
- **THEN** lighting draws SHALL still execute a SECONDARY command buffer inside that render pass
- **AND** the rate attachment texel size SHALL be `{1,1}` when the device allows it, otherwise the device minimum

### Requirement: Editor Viewport rate-mask overlay
The editor Viewport SHALL provide a session toggle that composites a two-colour decode of the rate image (one colour for 1×1, another for 2×2). The toggle SHALL be off by default and SHALL NOT persist across a fresh editor launch. The overlay SHALL NOT draw in Player, Camera Preview, Placement Preview, Mesh Preview, or Scene Thumbnail / Capture, regardless of toggle state. The overlay SHALL NOT be a product settings page.

#### Scenario: Toggle shows two rates
- **WHEN** the author enables the rate-mask overlay in the editor Viewport
- **AND** VRS is writing 1×1 and 2×2 texels
- **THEN** those two rates SHALL be visually distinguishable

#### Scenario: Player never shows the mask
- **WHEN** the rate-mask overlay is enabled in the editor
- **AND** the same scene runs as Player
- **THEN** the Player frame SHALL contain no rate-mask overlay
