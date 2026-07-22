## ADDED Requirements

### Requirement: C-ABI message register and send
The engine C-ABI SHALL expose `blunder_message_register`, `blunder_message_send`, `blunder_message_set_hook`, and `blunder_message_clear_hook` (names may match NativeAbi field spelling). Register SHALL accept a UTF-8 name and write a stable MessageId. Send SHALL accept ObjectId, MessageId, a pointer to at most four `BlunderMessageArg` values, and an argc in 0..4.

#### Scenario: ABI version is 4
- **WHEN** `blunder_engine_abi_version` is called after this change
- **THEN** it returns 4

#### Scenario: Native register and send without managed host
- **WHEN** a native test registers a name, sets a hook, creates an Object with a peer, and sends
- **THEN** the hook is invoked once per peer with the MessageId and args
