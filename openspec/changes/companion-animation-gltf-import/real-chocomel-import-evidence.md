# Real Chocomel Import evidence

Tasks 5.1a and 5.2a automate the Import-side gate with the real DogWalk
Chocomel files. Interactive Content Browser and Edit/Play checks remain
optional human smoke in `manual-checklist.md`; Tasks 5.1 and 5.2 stay open.

## Source pack

- Host: `E:\Godot Projects\dogwalk-repo\pro\game\assets\char\chocomel\Chocomel.gltf`
- Idle: `E:\Godot Projects\dogwalk-repo\pro\game\animations\world\LOOP-chocomel-idle\LOOP-chocomel-idle.gltf`
- Walk: `E:\Godot Projects\dogwalk-repo\pro\game\animations\world\LOOP-chocomel-walk\LOOP-chocomel-walk.gltf`
- Both companions use an external `.bin` beside the `.gltf`.

Set `BLUNDER_DOGWALK_GAME_ROOT` to override the default
`E:/Godot Projects/dogwalk-repo/pro/game` root. If any required real source
or sidecar is absent, the test prints `SKIP real Chocomel Import: missing ...`
and leaves CI green.

## Automated scenarios

`asset_import_test::importRealDogWalkChocomelSources` covers:

1. Multi-select host + idle + walk: exactly one Mesh descriptor, no LOOP Mesh
   descriptors, both stem-named AnimationClip descriptors registered by GUID,
   companion glTF and `.bin` files persisted under
   `resources/Models/Chocomel/companions/`, and two
   `companion_animation_sources` recorded.
2. Single disconnected host: Mesh Import succeeds, no LOOP clips are created,
   and `companion_animation_sources` is empty.
3. Co-located copy of the same real pack: single host Import discovers sibling
   child-folder companions and registers both clips.

The real host also exposed that external glTF dependencies were not copied
with the Mesh Intermediate. The regression assertion now requires
`Resources/Models/Chocomel/Chocomel.bin`; Import copies all non-data buffer and
image URIs beside the persisted host glTF.

## Verification

Run:

```powershell
cmake --build build/vs2026-debug --config Debug --target asset_import_test
.\build\vs2026-debug\engine\src\tests\Debug\asset_import_test.exe
```

Expected real-file marker:

```text
RUN real Chocomel Import from E:/Godot Projects/dogwalk-repo/pro/game
asset_import_test: all passed
```
