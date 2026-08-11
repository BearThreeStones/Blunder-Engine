## 1. Tests first (TDD)

- [x] 1.1 TDD: host+companion multi-select registers Mesh + Clips with Intermediate under `Resources/Animations/<stem>/` and **no** `companion_animation_sources` / **no** `Models/{host}/companions/`
- [x] 1.2 TDD: companion-only Import still registers Clips under `Animations/<stem>/` (not `_standalone_companions`)
- [x] 1.3 TDD: near-disk discovery registers independent Clips without Mesh packaging list
- [x] 1.4 TDD: Mesh Reimport does not mutate companion-derived Clip GUIDs/bodies; Clip Reimport refreshes from Clip `source` and preserves GUID
- [x] 1.5 TDD: delete Mesh leaves AnimationClip Assets registered
- [x] 1.6 TDD: migration moves `companions/` and `_standalone_companions/` into `Animations/<stem>/`, clears Mesh field, preserves Clip GUIDs

## 2. Import path rewrite

- [x] 2.1 Change Intermediate copy destination for companion/animation-only glTF to `Resources/Animations/<stem>/`
- [x] 2.2 Stop writing/reading `companion_animation_sources` as required packaging (ignore-on-load until migration clears)
- [x] 2.3 Multi-select / near-disk: register independent Assets; bone-mismatch warn vs single skinned host; no AnimationPlayer map auto-fill
- [x] 2.4 Align standalone orphan path with the same `Animations/<stem>/` layout (remove product use of `_standalone_companions`)

## 3. Reimport + delete

- [x] 3.1 Mesh Reimport refreshes Mesh Intermediate only (remove companion re-extract via packaging list)
- [x] 3.2 Clip Reimport uses Clip descriptor `source`
- [x] 3.3 Confirm / adjust delete Asset path so Mesh delete never cascades Clips via packaging metadata

## 4. Migration

- [x] 4.1 Implement one-shot project migrator (prefer `.blunder` stamp) for legacy companion folders + Mesh field cleanup
- [x] 4.2 Wire migrator on project open or Content Browser first refresh (per design open question: once-per-project stamp)

## 5. Docs + manual smoke

- [x] 5.1 Sync companion / standalone checklists with ADR 0028 paths (no Mesh-child Intermediate expected)
- [x] 5.2 Manual smoke: multi-select Chocomel + idle + walk -> Mesh + Clips under `Resources/Animations/…`; delete Mesh leaves clips; Clip Reimport works
