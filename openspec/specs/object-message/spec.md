# object-message Specification

## Purpose
Directed gameplay Messages between Objects: game-registered MessageIds, synchronous Send to one ObjectId, fan-out to Behaviour peers via OnMessage, managed Message façade over NativeAbi.

## Requirements

### Requirement: Game-registered MessageId
The engine SHALL map game-provided UTF-8 message names to stable MessageId values. Re-registering the same name SHALL return the same MessageId. The engine SHALL NOT ship a fixed gameplay vocabulary of MessageIds (Hit, Damage, Die, …) as builtins.

#### Scenario: Register is stable
- **WHEN** a name `"Hit"` is registered twice
- **THEN** both calls return the same non-zero MessageId

#### Scenario: Distinct names distinct ids
- **WHEN** `"Hit"` and `"Heal"` are registered
- **THEN** their MessageIds differ

### Requirement: Directed synchronous Send to one Object
`Message.Send` SHALL deliver a Message identified by MessageId to exactly one target ObjectId with at most four MessageArg values. Delivery SHALL be synchronous: when Send returns, all eligible Behaviour handlers for that Send have been invoked (or skipped). Invalid or destroyed ObjectIds SHALL result in no delivery and SHALL NOT fault.

#### Scenario: Send reaches Behaviours
- **WHEN** Object A Sends MessageId M with args to valid Object B that has two Behaviours with Script Peers
- **THEN** both Behaviours receive OnMessage for M in Behaviour list order before Send returns

#### Scenario: Invalid target is no-op
- **WHEN** Send targets an invalid ObjectId
- **THEN** no Behaviour OnMessage runs and the call completes without error

### Requirement: Fan-out to all Behaviours without consume-stop
Send SHALL snapshot the target Object's Behaviour list before invoking handlers. Every Behaviour slot with a non-null Script Peer at invoke time SHALL receive the Message. There SHALL be no Handled flag that stops later siblings. Null peers and peers that disappear after the snapshot SHALL be skipped.

#### Scenario: All peers notified
- **WHEN** three Behaviours with peers exist on the target and Send runs
- **THEN** OnMessage is invoked three times in list order

#### Scenario: Null peer skipped
- **WHEN** one Behaviour slot has no Script Peer
- **THEN** Send does not invoke the message hook for that slot

### Requirement: Behaviour OnMessage receive path
Blunder.Api Behaviour SHALL expose a virtual `OnMessage(MessageId id, ReadOnlySpan<MessageArg> args)` (or equivalent) as the sole MVP engine receive path. Per-name methods such as `OnHit` are NOT required for MVP.

#### Scenario: Override receives
- **WHEN** a Behaviour overrides OnMessage and a matching Message is Sent to its host Object
- **THEN** that override runs with the Sent MessageId and args

### Requirement: Global Message façade
Authors SHALL register and send via a global `Message` API (`Register` / `Send`), not via Object instance methods as the primary product surface. MVP callers are C# Behaviours; native code MAY call the same delivery path later without changing fan-out semantics.

#### Scenario: Register then Send from C#
- **WHEN** managed code Registers `"Ping"` and Sends it to another ObjectId
- **THEN** the target Behaviour OnMessage observes that MessageId

### Requirement: MessageArg bag limits and kinds
Each Send SHALL carry zero to four arguments. MVP wire kinds SHALL include Bool, Int (int64), Float, and ObjectId. Exceeding four arguments SHALL fail the Send (error return or throw) without partial delivery.

#### Scenario: Four args delivered
- **WHEN** Send supplies four Int args
- **THEN** OnMessage receives argc 4 with matching values

#### Scenario: Five args rejected
- **WHEN** Send is attempted with five args
- **THEN** delivery does not occur and the API reports failure

### Requirement: OnMessage Lifecycle exception does not stop fan-out
If `OnMessage` on one Behaviour throws, Send SHALL abort that invocation, record the Lifecycle exception as specified by debug-api, and SHALL still invoke remaining Behaviours in the snapshot. Send SHALL return after the snapshot has been walked.

#### Scenario: Second receiver still gets the Message
- **WHEN** Object B has two Behaviours with peers and the first OnMessage throws
- **THEN** the second Behaviour still receives OnMessage for that Send
- **AND** Send returns without ending the Play Process
