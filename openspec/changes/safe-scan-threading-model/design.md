## Context

The application is a C++20 Win32/WTL desktop executable. `ScanCoordinator` currently constructs a working `Database`, releases it to a raw pointer captured by a raw `std::thread`, and posts heap-allocated progress and completion messages to `MainFrame`. `MainFrame::HandleMessage(WM_SCAN_COMPLETE)` calls `scans_.Join()` before adopting or deleting the posted `Database`, and `MainFrame::OnDestroy()` also calls `Join()`. `CatalogSession` owns `pendingDatabase_` as a `std::unique_ptr`, but its safe UI-thread-only use is not protected by the worker/message contract.

The scan performs filesystem and database work that may take significant time. Closing the window or cancelling must therefore remain responsive even while the worker is unwinding. C++20 is already configured in `WhereIsThat.vcxproj`, so `std::jthread` and `std::stop_token` are available, but a `jthread` destroyed from a UI callback would still synchronously join and is not by itself a non-blocking design.

## Goals / Non-Goals

**Goals:**
- Give every scan an explicit generation id, cancellable state, and explicit `Completed`, `Failed`, or `Cancelled` outcome.
- Use C++20 cooperative cancellation throughout meaningful scan traversal and staging checkpoints.
- Remove raw owning pointer transfer through Win32 messages and make pending database adoption exactly once and UI-thread confined.
- Keep close, destroy, completion, and replacement-scan paths free of synchronous worker waits.
- Permit late, stale, duplicate, invalid, or post-destruction notifications to be discarded without touching destroyed UI or pending catalog state.

**Non-Goals:**
- Change what files are indexed, the database format, Save semantics, or the Add New Disk/Media dialog's accepted-source behavior.
- Provide pause/resume, concurrent scans, or persistence of in-progress work across process exit.
- Hide a long-running worker on process shutdown indefinitely; cancellation must still drain through a controlled non-UI-blocking lifecycle.

## Decisions

- Replace `ScanCoordinator`'s raw `std::thread` with an RAII scan-job abstraction implemented with `std::jthread` because the project already builds as C++20. Each `ScanJob` owns its stop state, immutable `ScanId`, working database result under construction, and outcome state. Alternative considered: a custom `std::thread` wrapper; it provides no benefit while C++20 is available and requires duplicating stop-token behavior.

- Make cancellation cooperative rather than attempting forced termination. `ScanCoordinator::RequestCancel()` requests stop on the active job; filesystem enumeration, progress checkpoints, before/after transaction mutations, and before result publication inspect `std::stop_token`. A cancellation path rolls back/discards the candidate database and publishes `Cancelled`, never a stageable database. Alternative considered: blocking join or terminating the worker; both threaten UI responsiveness or database integrity.

- Treat only one scan as active. A start request while a scan is active is rejected without starting a second worker; close explicitly requests cancellation. This preserves current single-scan UX and prevents concurrent working copies from racing to become pending state. A future replacement UX may request cancellation and start a new scan after the cancelled result is observed, using a new id. Alternative considered: immediately launching a replacement scan while the prior scan drains; it complicates I/O, messaging, and pending-state arbitration without being required.

- Allocate a monotonically increasing `ScanId` for each successfully launched job and include it in every progress/completion notification. `MainFrame` records the expected active id and applies completion only when it matches; messages for stale, duplicate, cancelled-after-replacement, or unknown ids are drained/discarded safely. Alternative considered: deriving identity from thread or message pointer addresses; addresses are reusable and do not express application ordering.

- Stop using `LPARAM` as ownership transport. The worker publishes a `ScanResult` containing `ScanId`, outcome, error information, and an optional `std::unique_ptr<Database>` into a synchronized coordinator/result-channel mailbox. `WM_SCAN_COMPLETE` carries only a non-owning scan-id notification; on the UI thread, `TakeResult(id)` removes one result from the mailbox and transfers its `unique_ptr` exactly once. Progress uses id-tagged non-owning snapshots or a similarly consumed mailbox entry, avoiding posted heap deletions. Alternative considered: posting a heap-allocated smart-pointer wrapper; that still depends on every window-message path freeing an owned payload and is fragile when a window is destroyed.

- Confine `CatalogSession::pendingDatabase_` to the UI thread. The worker never reads or writes it; it mutates only its private working database. Only a validated current-generation `Completed` result may call `AcceptPending(std::unique_ptr<Database>)`; `Failed`, `Cancelled`, stale, missing, or already-consumed results cannot replace pending state. Alternative considered: mutex protection of `pendingDatabase_`; confinement is simpler because all catalog presentation and Save actions already occur on the UI thread.

- Deliver job notifications through a lifetime-safe UI delivery boundary. The implementation SHALL use either an application-owned message sink/dispatcher that remains alive until outstanding jobs drain, or an equivalent registered sink whose lifetime gate prevents a worker from posting to/reaching a destroyed `MainFrame`. The dispatcher/UI lookup validates the current registered frame and scan id before invoking UI behavior. Alternative considered: continuing to capture the main window `HWND` directly in the worker; late delivery and possible handle reuse leave destruction behavior under-specified.

- Separate worker completion from worker reclamation. A completion handler may consume a published result and update UI state, but it SHALL NOT call `join()` or destroy the last `std::jthread` owner where destruction would join on that handler stack. Completed/cancelled jobs are handed to a non-UI reaping owner or retained until controlled application teardown after interactive window processing has ended. Alternative considered: resetting a `std::jthread` in `WM_SCAN_COMPLETE`; its implicit join is still a synchronous UI wait.

- Model close as an asynchronous cancellation state. When close is requested during a scan, `MainFrame` records closing/cancellation-pending state, disables scan-affecting controls, requests cancellation, and returns immediately. Normal closure can finish after the matching result is consumed; if the frame is destroyed through another route, it unregisters from result delivery before UI resources are released and the job is safely reaped without dereferencing frame state. Alternative considered: refusing close until natural completion; it leaves the user waiting for work that should be cancellable.

- Represent outcomes explicitly as `Completed`, `Failed`, and `Cancelled`. `Completed` is the only result that may contain a pending database for adoption; `Failed` contains displayable failure context and leaves the saved/pending catalog unchanged; `Cancelled` clears busy/cancelling presentation without an error crash path. Alternative considered: a boolean success flag; it conflates user cancellation with an error and weakens ownership invariants.

## Risks / Trade-offs

- `[A std::jthread destructor joins even when cancellation was requested]` -> Do not destroy/reassign the last job owner inside UI event handling; establish a reaper or post-message-loop owner and test close while a slow worker drains.
- `[FileScanner may not currently expose cancellation checkpoints]` -> Extend its scanning contract to accept a stop token/callback and check during traversal and expensive work, with cancellation regression coverage.
- `[Messages can arrive after cancellation or out of order]` -> Validate scan ids and consume results once from a synchronized mailbox; stale notification processing has no UI/catalog side effect.
- `[An HWND captured by the worker could be invalid or reused]` -> Route delivery through a lifetime-controlled dispatcher/registration boundary rather than direct frame access.
- `[A close request may briefly leave a visible cancelling window]` -> Disable conflicting actions and show cancellation-pending status while preserving UI responsiveness until safe closure completes.

## Migration Plan

1. Define scan id, stop-aware job/outcome, mailbox, and lifetime-safe result-delivery interfaces alongside the current scan entry point.
2. Make `FileScanner` and transaction staging observe cooperative cancellation and convert working database ownership to result-owned RAII.
3. Move `MainFrame` message handling to scan-id validation and `TakeResult()` adoption, including explicit UI state for failed/cancelled/closing results.
4. Remove all UI-callback joins and direct raw message ownership; arrange off-handler reclamation/application teardown for completed jobs.
5. Exercise normal, cancelled, failed, stale, malformed, rapid-close, and destruction-with-late-completion paths before removing obsolete message structures/APIs.

Rollback restores the previous coordinator/message path only as a source rollback; there is no persisted-data migration because successfully accepted pending database semantics and catalog format do not change.

## Open Questions

- Should the lifetime-safe notification boundary be an application-owned message-only window or a coordinator dispatcher integrated with the existing WTL message loop?
- Which test hook best produces a deterministically slow/cancellable filesystem scan for shutdown and stale-generation regression tests?
