## 1. Acceptance + discovery helpers

- [x] 1.1 TDD: companion acceptance — animations ∧ meshes=0 (skins allowed); reject skinned mesh-with-geometry
- [ ] 1.2 TDD: near-disk candidate enumeration (mesh dir + parent immediate child dirs); exclude self; no deep recursion
- [ ] 1.3 TDD: multi-select batch pairing — one skinned host; companions attach; orphans warn; multiple skinned hosts split

## 2. Import integration

- [ ] 2.1 Wire `importExternalFiles` / mesh Import: host + companions when animations enabled; skip companions when animations disabled
- [ ] 2.2 Copy accepted companions under Resources Intermediate (not Mesh Assets); record paths for Reimport (descriptor list or convention — pick one, document)
- [ ] 2.3 Extract/register clips from companions via existing extractor; clip names prefer companion file stem; bone mismatch warn+register
- [ ] 2.4 Near-disk discovery on single-mesh Import (secondary); merge with embedded clips under same mesh stem

## 3. Reimport

- [ ] 3.1 Reimport host Mesh refreshes clips from mesh Intermediate + stored companion Intermediate; preserve stable clip GUIDs
- [ ] 3.2 Tests: Reimport companion-derived clip GUID stability

## 4. Validation + content unblock

- [ ] 4.1 Integration test fixture: synthetic mesh (skins, no anim) + companion LOOP-shaped glTF (anim, meshes=0) multi-select Import → Mesh + Clip Assets
- [ ] 4.2 Document Chocomel multi-select Import steps for Test Project (paths under dogwalk-repo); do not claim Phase Done until Play acceptance elsewhere
- [ ] 4.3 Confirm CONTEXT + ADR 0021 match apply (prefer no churn)

## 5. Manual checklist

- [ ] 5.1 Content Browser: multi-select Chocomel + idle + walk → clips appear; Play/Edit can address stem names
- [ ] 5.2 Co-located near-disk pack (if available) still attaches; disconnected trees without multi-select do not falsely attach
