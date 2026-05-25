## 1. Lifecycle And Ownership Audit

- [ ] 1.1 Audit `ScanCoordinator`, `MainFrame`, `CatalogSession`, `FileScanner`, and all `WM_SCAN_PROGRESS`/`WM_SCAN_COMPLETE` paths for thread creation, waits, raw payload ownership, database access, and shutdown entry points.
- [ ] 1.2 Document and encode the invariant that `pendingDatabase_` is owned and mutated only on the UI thread after validated exactly-once completed-result adoption.
- [ ] 1.3 Select and wire the lifetime-safe notification sink/reaper ownership boundary so outstanding jobs can drain after `MainFrame` is closing or destroyed without an event-handler join.

## 2. Cancellable Worker And Result Channel

- [ ] 2.1 Replace `ScanCoordinator` raw `std::thread` ownership and public `Join()` dependency with a `std::jthread`-based scan job abstraction, unique scan ids, and explicit active/cancelling state.
- [ ] 2.2 Introduce `std::stop_token` cooperative cancellation through scan staging and `FileScanner` traversal/checkpoints, including rollback/discard behavior for cancellation.
- [ ] 2.3 Define explicit `Completed`, `Failed`, and `Cancelled` result types with RAII ownership of any completed working database and useful failure context.
- [ ] 2.4 Replace raw owning pointer `PostMessage` payloads with id-tagged non-owning notifications plus a synchronized result/progress mailbox that supports exactly-once result consumption.

## 3. UI Integration And Shutdown Behavior

- [ ] 3.1 Refactor `WM_SCAN_COMPLETE` and progress handling to validate scan ids and result availability, ignore stale/duplicate/invalid messages safely, adopt only valid completed databases, and surface failed/cancelled outcomes appropriately.
- [ ] 3.2 Add active generation and cancellation-pending UI state so conflicting scan controls are disabled while the current scan is active or draining and replacement scan requests cannot race pending state.
- [ ] 3.3 Refactor `OnClose`, `OnDestroy`, and completion handling to request cancellation/unregister result delivery as needed without synchronously waiting for or destroying a joinable worker on the UI callback stack.
- [ ] 3.4 Verify a late worker publication after UI teardown is reclaimed by the lifecycle owner and cannot access frame controls, catalog presentation, or `pendingDatabase_`.

## 4. Verification And Regression Coverage

- [ ] 4.1 Add focused automated tests or test seams for normal completion, explicit failure, cooperative cancellation, exactly-once pending database adoption, and cancellation leaving pending state unchanged.
- [ ] 4.2 Add regression tests for stale generation completion, duplicate completion, invalid/null-equivalent completion notification, rapid start/close, and worker completion after UI destruction.
- [ ] 4.3 Audit the completed change to confirm no raw owning pointers pass through `WM_SCAN_COMPLETE` and no UI event handler calls `Join()` or otherwise performs an implicit synchronous worker wait.
- [ ] 4.4 Build the supported project configurations and run available tests plus static analysis or thread-safety/race analysis tooling where supported, recording any unavailable sanitizer limitation for the Windows toolchain.
