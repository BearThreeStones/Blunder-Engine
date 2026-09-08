## Purpose

Process-wide run-to-completion CPU Jobs for Editor and Player, with fork-join barriers and wait-help, without Fibers or a tick-path caller in this slice.

## ADDED Requirements

### Requirement: Job System is a Context System
Every shipped Host composition SHALL start a Job System at process boot and SHALL tear it down only at process shutdown. Editor Session and Player SHALL both start it, including Headless. The Job System SHALL NOT be Privileged core, SHALL NOT be a Seam, and SHALL NOT be omitted from the Player.

#### Scenario: Headless Editor mounts Job System
- **WHEN** a Headless Editor Session starts
- **THEN** a Job System is present for that process

#### Scenario: Headless Player mounts Job System
- **WHEN** a Headless Player starts
- **THEN** a Job System is present for that process

### Requirement: Independent Jobs finish at a Job barrier
The Job owner thread SHALL Submit independent Jobs and SHALL enter a Job barrier that returns only after that submitted set has finished. A Job SHALL run to completion and SHALL NOT wait, yield, Submit, or enter a barrier. v1 SHALL NOT declare Job-to-Job dependency edges. The waiting owner thread SHALL run remaining Jobs (help) until the barrier returns.

#### Scenario: Zero dedicated Workers complete via help
- **WHEN** a Job System is constructed with 0 dedicated Workers
- **AND** the owner thread Submits a batch of independent Jobs that write caller-owned buffers
- **AND** the owner thread enters a Job barrier
- **THEN** every Job in that batch SHALL have finished
- **AND** the barrier SHALL return
- **AND** the process SHALL NOT hang

#### Scenario: Default dedicated Workers complete the same batch
- **WHEN** a Job System is constructed with the default dedicated Worker count
- **AND** the owner thread Submits a batch of independent Jobs that write caller-owned buffers
- **AND** the owner thread enters a Job barrier
- **THEN** every Job in that batch SHALL have finished
- **AND** the barrier SHALL return

### Requirement: Single Job owner thread in v1
Only the thread that starts the Job System SHALL Submit Jobs or enter a Job barrier. Dedicated Workers SHALL run Jobs and SHALL NOT Submit or enter a barrier.

#### Scenario: Owner thread is the only Submitter
- **WHEN** tests Submit and Wait on the thread that constructed the Job System
- **THEN** those operations SHALL succeed
- **AND** dedicated Workers SHALL NOT Submit or Wait

### Requirement: Dedicated Worker count may be zero
The default dedicated Worker count SHALL be `max(0, hardware_concurrency - 1)`. A Job System with 0 dedicated Workers SHALL be a supported configuration. Zero dedicated Workers SHALL NOT deadlock a Job barrier.

#### Scenario: Default count leaves one core for the owner
- **WHEN** the Job System starts with the default dedicated Worker count on a machine with N logical processors
- **THEN** dedicated Worker count SHALL be `max(0, N - 1)`

### Requirement: Jobs only touch Job data
A Job SHALL read and write only caller-owned bytes prepared before Submit and consumed after the barrier. A Job SHALL NOT call into Object, ClassDB, SceneInstance, RHI, or Slint.

#### Scenario: Jobs write caller buffers
- **WHEN** the owner thread prepares integer slots, Submits one Job per slot that writes a distinct value, and enters a Job barrier
- **THEN** each slot SHALL contain the value written by its Job after the barrier returns

### Requirement: Jobs have no error channel
A Job SHALL NOT throw and SHALL NOT return a status to the caller. A Job barrier SHALL mean the batch finished, not that the work was correct.

#### Scenario: Barrier is completion not correctness
- **WHEN** a batch of Jobs that write caller buffers finishes
- **THEN** the Job barrier SHALL return
- **AND** the Job System SHALL NOT report a per-Job success or error code

### Requirement: Shutdown joins dedicated Workers
Process shutdown SHALL stop accepting Jobs, finish or drop in-flight Jobs on the owner thread path, and join dedicated Workers so the process can exit.

#### Scenario: Shutdown does not hang
- **WHEN** a Job System with dedicated Workers shuts down on the owner thread after a barrier has returned
- **THEN** dedicated Workers SHALL join
- **AND** shutdown SHALL return

### Requirement: v1 has no script or C-ABI Job Submit
v1 SHALL NOT add a C-ABI or Blunder.Api function that Submits Jobs or enters a Job barrier. Behaviours SHALL NOT schedule Jobs.

#### Scenario: No managed Job schedule
- **WHEN** a Behaviour Ticks
- **THEN** that Tick SHALL still run on the engine tick thread
- **AND** there SHALL be no C-ABI Job Submit entry for that Behaviour to call
