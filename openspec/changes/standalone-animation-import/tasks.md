## 1. Import path (TDD)

- [x] 1.1 TDD: single companion-only glTF → AnimationClip Asset by stem; no Mesh
- [x] 1.2 TDD: two orphan companions → two clips; multi-host orphans do not attach to wrong host
- [x] 1.3 TDD: host+companion multi-select pairing still registers Mesh + clips as today
- [x] 1.4 TDD: animations=false does not register orphan clips

## 2. Service wiring

- [x] 2.1 Implement orphan → `extractAndRegisterAnimationClipsFromGltf` in `importExternalFiles`
- [x] 2.2 Persist Intermediate as needed for durable clip `source`; refresh Content Browser
- [x] 2.3 Log clear standalone vs paired messages (replace skip-only orphan warn)

## 3. Docs / ADR

- [x] 3.1 Note standalone companion Import in companion checklist / ADR 0021 see-also
- [ ] 3.2 Manual smoke: Import Chocomel Mesh alone, then Import each LOOP alone → clips appear; bind in Inspector
