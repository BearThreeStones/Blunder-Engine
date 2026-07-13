# Inspector rotation is Quaternion-authoritative with a fixed Euler view

Entity rotation remains a Quaternion. The Inspector Rotation Edit Mode (Euler vs Quaternion) only changes presentation and editing controls. Euler fields recompose through the engine's existing SceneSerializer convention (`qz * qy * qx` on world X/Y/Z) so the Inspector matches scene assets and current editor behavior. Quaternion component edits normalize on commit. There is no Rotation Order dropdown and no Basis mode in v1 — reversing this would fork Inspector math from scene I/O or reintroduce dual rotation stores.
