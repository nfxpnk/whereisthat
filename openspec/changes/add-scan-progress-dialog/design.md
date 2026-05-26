## Context

Add/Update media selection starts a `ScanCoordinator` worker through `CatalogWorkflowController`. The coordinator already posts UI-thread progress notifications containing cumulative file and folder counts, while `MainFrame` renders controller effects. `src/ui/ProgressDialog.*` exists as an unused placeholder.

## Goals / Non-Goals

**Goals:**
- Present an unobtrusive scan dialog throughout an accepted background scan.
- Show cumulative file and folder counts without allowing worker code to access window controls.
- Allow the user to cancel the active scan from that dialog through the existing coordinated stop/rollback path.
- Allow developers to make short test scans observable through an explicitly disabled-by-default compile-time delay.

**Non-Goals:**
- Scan queueing or pausing/resuming a scan.
- Persistence/schema changes or changes to staged-scan ownership.
- Configuration UI for the developer delay.

## Decisions

- Implement `ProgressDialog` as a modeless WTL dialog owned by `MainFrame`. A modal dialog would block message delivery needed to display worker progress, and worker ownership remains in the controller/coordinator layers.
- Extend controller results with a small scan-dialog presentation effect (`show`, `update`, `close`, counts). The controller knows when a scan was actually accepted and when its active result is complete; rendering directly from worker callbacks would cross the existing separation between behavior and Win32 presentation.
- Have the progress dialog expose a Cancel callback to `MainFrame`, which requests cancellation from `CatalogWorkflowController`; the dialog displays `Cancelling...` and disables repeat requests until the worker posts completion. This reuses `ScanCoordinator::RequestCancel()` and its existing staged rollback behavior rather than making UI code manage worker state.
- Keep pacing constants local to `FileScanner.cpp`, disabled by default, with the duration expressed in microseconds. While the delay is enabled, publish progress for every encountered item so a deliberately slow, small scan visibly updates; normal scans retain batched reporting to avoid unnecessary UI traffic.

## Risks / Trade-offs

- [Modeless status window can remain open if completion is mishandled] -> Close it from the same active-scan completion path that resets status state, and destroy it during main-frame destruction.
- [Cancellation is asynchronous] -> Keep the dialog visible in a disabled `Cancelling...` state until completion confirms the worker has retired.
- [A delay enabled for a production build slows all scans] -> Define the switch as an obvious disabled-by-default compile-time constant adjacent to the delay duration.
- [Per-item notification volume would hurt large normal scans] -> Use item-by-item publication only with test delay enabled; retain existing periodic updates otherwise.
