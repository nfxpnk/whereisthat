## ADDED Requirements

### Requirement: Cancellable RAII scan execution
The system SHALL execute each active catalog scan in a C++20 RAII-managed worker with a unique scan identifier and cooperative cancellation support, and SHALL allow no more than one active scan to produce a pending result at a time.

#### Scenario: Starting a scan creates a cancellable worker
- **WHEN** an eligible Add/Update operation starts a scan
- **THEN** the system allocates a new scan id and starts an RAII-managed cancellable worker for that scan
- **THEN** the worker observes cancellation state during scan processing and before publishing a stageable result

#### Scenario: Starting a second scan while one is active
- **WHEN** the user requests another scan while a prior scan is active or cancelling
- **THEN** the system rejects the new start or requests cancellation of the prior scan before any replacement can start
- **THEN** no two active scans can update pending catalog state

#### Scenario: Cancellation is requested during scanning
- **WHEN** cancellation is requested while the worker is traversing or staging data
- **THEN** the worker observes cancellation at cooperative checkpoints
- **THEN** the worker produces a `Cancelled` outcome rather than a stageable completed result

### Requirement: Non-blocking UI lifecycle handling
The system SHALL keep all UI event handling non-blocking with respect to scan worker completion and SHALL perform worker cancellation and eventual reclamation without a synchronous join in a window event handler.

#### Scenario: Closing the window during a scan
- **WHEN** the user requests window close while a scan is active
- **THEN** the UI requests cancellation and marks scan controls unavailable while cancellation is pending
- **THEN** the close handler returns without waiting synchronously for the scan worker to finish

#### Scenario: Completion is delivered to the UI
- **WHEN** `WM_SCAN_COMPLETE` is handled for any scan notification
- **THEN** the handler processes or discards the notification without calling `Join()` or otherwise synchronously waiting for worker completion

#### Scenario: UI destruction occurs while a worker remains outstanding
- **WHEN** the window is destroyed while a scan completion or cancellation can still arrive
- **THEN** destruction unregisters or invalidates the UI delivery target without calling `Join()` from `OnDestroy`
- **THEN** worker cleanup is completed through a lifetime-safe asynchronous or post-window teardown owner

### Requirement: Safe generation-tagged completion delivery
The system SHALL deliver scan progress and completion through non-owning notifications associated with a scan id and SHALL validate notification identity and result availability before changing user-visible or catalog state.

#### Scenario: Current completion is received
- **WHEN** `WM_SCAN_COMPLETE` identifies the currently expected active scan and its result is present
- **THEN** the UI consumes that scan result at most once and updates status according to its explicit outcome

#### Scenario: WM_SCAN_COMPLETE identifies a stale scan
- **WHEN** `WM_SCAN_COMPLETE` contains an id that is not the UI's currently expected active scan id
- **THEN** the completion is ignored or drained safely
- **THEN** it does not alter `pendingDatabase_`, visible catalog contents, or current scan status

#### Scenario: WM_SCAN_COMPLETE has a null or invalid notification
- **WHEN** `WM_SCAN_COMPLETE` contains an invalid id, missing result, malformed notification value, or an already-consumed result
- **THEN** the handler safely discards it without dereferencing or deleting an owning raw pointer
- **THEN** the application continues without crashing or modifying pending catalog state

#### Scenario: Worker completes after UI destruction
- **WHEN** a scan worker finishes after its main UI target has been destroyed or unregistered
- **THEN** its result is discarded or reclaimed through the lifecycle-safe result owner
- **THEN** it does not post to, dereference, or invoke state on the destroyed UI object

### Requirement: Exactly-once pending database ownership
The system SHALL keep a worker-produced database in RAII-managed result ownership until a validated completed result transfers it exactly once to UI-thread-confined pending catalog state.

#### Scenario: A completed scan transfers database ownership
- **WHEN** the current scan completes successfully with a stageable working database
- **THEN** the UI thread transfers ownership of that database to `pendingDatabase_` exactly once
- **THEN** the accepted pending catalog becomes available through the existing staged Save workflow

#### Scenario: A cancelled scan produces no pending update
- **WHEN** the current scan reports a `Cancelled` outcome
- **THEN** no database from that scan is transferred to `pendingDatabase_`
- **THEN** existing pending or saved catalog data is not modified by the cancelled scan

#### Scenario: A duplicate completion is delivered
- **WHEN** a completion notification is delivered more than once for the same scan id
- **THEN** at most the first valid completed result can transfer database ownership
- **THEN** subsequent notifications cannot replace, delete, or re-adopt pending catalog state

### Requirement: Explicit scan outcome reporting
The system SHALL distinguish completed, failed, and cancelled scan outcomes and SHALL surface current-scan errors through safe UI behavior without staging failed work.

#### Scenario: A scan fails
- **WHEN** the current scan cannot prepare, traverse, commit, or publish a valid staged result
- **THEN** it reports a `Failed` outcome with user-presentable error information
- **THEN** the UI reports the failure without crashing and without updating `pendingDatabase_`

#### Scenario: A scan is cancelled
- **WHEN** the UI consumes a valid `Cancelled` outcome for the current scan
- **THEN** it clears or advances the corresponding busy/cancellation state without presenting the cancellation as a successful pending edit

#### Scenario: A scan succeeds
- **WHEN** the UI consumes a valid `Completed` outcome for the current scan
- **THEN** it updates the catalog presentation from the adopted pending database according to existing staged-save behavior
