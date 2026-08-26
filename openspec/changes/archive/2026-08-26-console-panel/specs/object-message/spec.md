## ADDED Requirements

### Requirement: OnMessage Lifecycle exception does not stop fan-out
If `OnMessage` on one Behaviour throws, Send SHALL abort that invocation, record the Lifecycle exception as specified by debug-api, and SHALL still invoke remaining Behaviours in the snapshot. Send SHALL return after the snapshot has been walked.

#### Scenario: Second receiver still gets the Message
- **WHEN** Object B has two Behaviours with peers and the first OnMessage throws
- **THEN** the second Behaviour still receives OnMessage for that Send
- **AND** Send returns without ending the Play Process
