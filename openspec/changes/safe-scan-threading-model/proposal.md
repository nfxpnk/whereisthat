## Why

Scanning currently crosses the worker/UI boundary with a raw `std::thread`, heap pointers posted in window messages, and synchronous `Join()` calls from UI event handlers. This leaves the highest-priority stability path vulnerable to frozen shutdown, unsafe late completions, and ownership races around pending catalog state.

## What Changes

- Replace scan worker lifetime management with a C++20 `std::jthread`-based, cooperatively cancellable lifecycle and explicit scan generation identity.
- Make scan completion, failure, cancellation, and progress delivery safe across the worker/UI boundary without transferring raw owning pointers in `WM_SCAN_COMPLETE`.
- Confine adoption of scanned pending catalog data to validated, current-generation UI handling so cancelled, stale, duplicate, or late results cannot mutate `pendingDatabase_`.
- Change close, destroy, new-scan, and completion behavior so UI message handlers request cancellation or process results without synchronously joining a worker.
- Define non-blocking shutdown behavior and safe dropping of results after the window has begun teardown or is destroyed.
- Add focused verification for cancellation, stale/invalid completion messages, ownership transfer, rapid start/close behavior, failure reporting, and asynchronous teardown.

## Capabilities

### New Capabilities
- `safe-scan-lifecycle`: Defines cancellable scan worker execution, generation-tagged result delivery, safe pending database adoption, and non-blocking UI/shutdown handling.

### Modified Capabilities

## Impact

- `src/app/ScanCoordinator.h` and `src/app/ScanCoordinator.cpp`: worker ownership, cancellation, scan identifiers, result envelope, posting policy, and teardown.
- `src/app/MainFrame.h` and `src/app/MainFrame.cpp`: `WM_SCAN_PROGRESS`/`WM_SCAN_COMPLETE` processing, command eligibility during cancellation, error/cancel status handling, `OnClose`, and `OnDestroy`.
- `src/app/CatalogSession.h` and `src/app/CatalogSession.cpp`: documented ownership/confinement rules for `pendingDatabase_` and exactly-once accepted result adoption.
- `src/core/FileScanner.*` or its call contract: cooperative stop checks during traversal if cancellation cannot be expressed at the coordinator boundary alone.
- Build/test/static-analysis configuration: lifecycle regression coverage and applicable thread-safety analysis; no database format or user-facing scan content semantics change is intended.
