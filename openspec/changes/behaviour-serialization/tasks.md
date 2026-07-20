## 1. Schema + serializer

- [x] 1.1 Extend `SceneEntityDefinition` with ordered Behaviour declarations (type, BehaviourId, optional property bag)
- [x] 1.2 Parse/write `behaviours` in `scene_serializer` (legacy scenes without the key remain valid)
- [x] 1.3 Unit test: JSON round-trip of Behaviour list on an entity

## 2. Instantiate: Object bind + slots

- [x] 2.1 On scene instantiate, for entities with Behaviours: create/bind Object to EntityId and restore slots/ids
- [x] 2.2 Advance Object next BehaviourId past max restored id
- [x] 2.3 Test: load without DotNetHost yields slots with null peers

## 3. Mount when host running

- [ ] 3.1 After load (or when host starts with scene already loaded): AttachBehaviour for each slot in order
- [ ] 3.2 Apply minimal property bag after attach (bool/number/string) or document skip if type has no bag
- [ ] 3.3 Test: load + host + fixture type → managed Tick / peer non-null (`editor_dotnet_host_test` style)

## 4. Export / save

- [ ] 4.1 EntityId→Object lookup for export
- [ ] 4.2 Write Behaviour list from Object when exporting Scene from live instance
- [ ] 4.3 Confirm tombstoned entities still omitted (including behaviours)

## 5. Docs + verify

- [ ] 5.1 Update testing.md / ADR 0011 or CONTEXT milestone note for Behaviour serialization slice
- [ ] 5.2 Mark dependency: requires `unify-script-objectdb` merged or stacked
- [ ] 5.3 Run serializer + mount tests green
