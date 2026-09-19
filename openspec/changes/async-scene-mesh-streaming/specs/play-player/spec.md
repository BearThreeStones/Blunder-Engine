# Spec Delta

## ADDED Requirements

### Requirement: Player first Iterate is not gated on unique Mesh residency
When the Player starts a Play entry Scene Asset, it SHALL instantiate that scene’s entity table and SHALL enter the process Iterate loop without waiting until every unique Mesh Asset in that scene is CPU-resident and GPU-resident. Unique meshes SHALL continue to become resident after Iterate has begun. The Player SHALL stream in its own process and SHALL NOT receive the editor Live SceneInstance as the Play world.

#### Scenario: Play Iterate before all unique meshes
- **WHEN** `engine_player` starts with a Scene Asset that has many unique Mesh Asset GUIDs
- **THEN** the Player reaches SDL Iterate with that scene’s entities instantiated
- **AND** unique Mesh CPU+GPU residency MAY still be in progress
- **AND** the Player world is not a pointer into the editor Live document

#### Scenario: Later frames fill missing unique meshes
- **WHEN** unique Mesh Assets become GPU-resident after Player Iterate has started
- **THEN** those MeshRenderers appear in the Player view
- **AND** the process does not restart to show them
