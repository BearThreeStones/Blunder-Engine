## MODIFIED Requirements

### Requirement: Reimport
Reimport SHALL refresh an existing Asset: if an archived Source path exists, re-run Source Export into Intermediate as defined by current Intermediate rules; otherwise refresh from the Asset’s Intermediate `source` (Intermediate-direct), including AnimationClip Reimport from the Clip descriptor `source`; then invalidate Finals and dependents. Reimport SHALL preserve the Asset GUID. Reimport SHALL remain invocable manually and MAY be invoked by Asset Watch through Detection Action. Mesh Reimport SHALL NOT refresh AnimationClip Assets via companion packaging metadata.

#### Scenario: Reimport preserves GUID
- **WHEN** Reimport runs for an Asset
- **THEN** the Asset GUID is unchanged and dependents still resolve

#### Scenario: Reimport from archived Source
- **WHEN** Reimport runs for an Asset with an archived Source path
- **THEN** Intermediate is regenerated from that Source and Finals are marked stale

#### Scenario: Intermediate-direct Reimport
- **WHEN** Reimport runs for an Asset whose descriptor has Intermediate `source` and no usable archived Source refresh path
- **THEN** Intermediate-derived data for that Asset is refreshed from `source` and the GUID is unchanged

#### Scenario: Detection may invoke Reimport
- **WHEN** Detection Action confirms or auto-runs for attributed GUIDs
- **THEN** Reimport runs for those GUIDs through the same Reimport entry points as manual Reimport
