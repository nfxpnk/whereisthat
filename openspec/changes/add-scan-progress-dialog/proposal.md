## Why

Add/Update Disk Image currently starts asynchronous scanning without a dedicated view of what is happening. Small scans are particularly difficult to observe while developing or verifying progress behavior.

## What Changes

- Show a scan-status dialog after an accepted Add/Update Disk Image operation starts.
- Display live scanned file and folder counts in that dialog while the background worker runs.
- Provide a Cancel button in the scan-status dialog to stop the active scan without accepting partial staged content.
- Close the scan-status dialog when scanning completes or is cancelled.
- Add compile-time constants for enabling an optional microsecond delay between scanned files so the progress UI can be exercised on small folders.

## Capabilities

### New Capabilities
- `scan-progress-feedback`: User-visible progress and developer test pacing for Add/Update media scanning.

### Modified Capabilities

## Impact

- `src/ui` gains a modeless scanning dialog.
- `src/app` connects scan start, progress, and completion effects to that dialog.
- `src/core/FileScanner.cpp` gains optional compile-time scan pacing and test-friendly progress notification behavior.
- Native dialog resources and the existing Win32/MSVC build are updated; storage schema and saved catalog contents do not change.
