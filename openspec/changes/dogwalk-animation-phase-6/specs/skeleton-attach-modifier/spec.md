## ADDED Requirements

### Requirement: SkeletonAttachModifier child Object follow
The engine SHALL provide a ClassDB **SkeletonAttachModifier** that, after Skeleton sample and within the modifier chain, copies a host attachment bone's world transform onto a configured **child Object**'s Transform so the child follows the bone. This SHALL NOT animate a remote Skeleton (not cross-Object Skeleton drive).

#### Scenario: Child follows bone
- **WHEN** SkeletonAttachModifier is enabled with a valid host bone and child Object and the host Skeleton pose updates
- **THEN** the child Object Transform matches the attachment bone world transform within tolerance after the modifier runs

#### Scenario: Not remote Skeleton drive
- **WHEN** SkeletonAttachModifier runs
- **THEN** it does not sample or advance another Object's Skeleton as an AnimationPlayer/Tree target

### Requirement: Invalid attach configuration fails safely
When the child Object or bone name is missing/invalid, SkeletonAttachModifier SHALL fail safely without crashing and SHALL NOT leave undefined engine state.

#### Scenario: Missing child
- **WHEN** the configured child ObjectId is invalid
- **THEN** the modifier skips writing Transform and reports a clear error/log path suitable for tests
