## ADDED Requirements

### Requirement: Flattened forest meshes are registered Assets
SE/SL library and set glTFs that the flatten bake copies into the DogWalk Project SHALL be registered Mesh Assets with GUIDs. The forest Scene Asset SHALL reference those GUIDs. Load SHALL use the existing Pull path (Fast Path Intermediate and Cook Final when fresh). The bake SHALL NOT leave the Scene Asset pointing only at unregistered Godot paths outside the Project.

#### Scenario: Scene depends on registered Mesh Assets
- **WHEN** the bake writes a layout instance that uses a library glTF
- **THEN** a Mesh Asset GUID exists in the DogWalk registry for that library asset
- **AND** the Scene Asset mesh field stores that GUID

#### Scenario: Cook or Fast Path loads the mesh
- **WHEN** the forest Scene Asset is instantiated and a referenced Mesh Asset has Intermediate in the Project
- **THEN** attach uses that Mesh Asset’s Intermediate or fresh Final
- **AND** it does not require the original Godot project tree to remain on the load path

### Requirement: Only reachable SE/SL content is pulled
The bake SHALL Import reachable SE/SL library glTFs under `assets/lib/**` and set glTFs under `assets/sets/{hub,fence,clearing,world}/**` (plus their `.bin` and textures needed to draw). It SHALL NOT pull ikea, zoo, or vertical_slice suites as part of this Scene Asset.

#### Scenario: Set and lib copy in, ikea stays out
- **WHEN** the bake copies content into DogWalk
- **THEN** reachable hub/fence/clearing/world set meshes and SE/SL library meshes are registered
- **AND** `SE-asset_ikea` / `SE-asset_zoo` meshes are not registered solely by this bake
