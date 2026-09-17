# Human acceptance — volumetric-fog

Status: **Not run**. Walk these in a windowed Player (Headless Play frame optional for story 1). Do not mark passed here.

| # | User story | Status |
|---|------------|--------|
| 1 | **Player shows volumetric fog** — Play a scene with Fog enabled. The Player game view (and Headless Play frames) show distance fog on lit meshes and empty sky via Forward composite `color * T + inscatter`. The editor Viewport may stay unfogged. | Not run |
| 2 | **Camera 3D volume, Z-up height** — Fog lives in a camera-aligned 3D volume of about 16-pixel screen tiles by 64 exponential Z slices. Density falls off with world Z (up). There is no separate 2D height-fog fullscreen pass. | Not run |
| 3 | **Unshadowed directional, thin temporal** — The volume is lit by the scene Directional with no volumetric shadows. A thin temporal blend (~20% current) calms slice banding. Point and spot lights do not inject. | Not run |
| 4 | **Stack on Forward Player** — Fog stacks on the existing Forward Player path. No new GBuffer. | Not run |

## Suggested setup

- Scene with a Camera, an illuminating Directional, Add… Fog (defaults), and a few opaque meshes at different distances / heights.
- Point/Spot present for story 3 (they must not add fog lighting).
- Compare editor Viewport vs Player of the same scene for story 1.
