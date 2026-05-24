## Why

The main window currently uses a single transient status-bar message and commits Add/Update scan results directly to the active catalog database as soon as scanning completes. Users need persistent at-a-glance catalog and selection context, and they need catalog changes to remain reviewable and discardable until an explicit Save.

## What Changes

- Divide the main-window status bar into five logical sections for catalog state, catalog protection, focused-item details, selected-item totals, and program-status lights.
- Display `Loaded` or `Modified` for the active catalog, show a narrow lock indicator for a read-only/protected catalog, and update file focus/selection summaries from the main contents view using compact formatted metadata.
- Add placeholder grey and green program-status lights while introducing app-wide `Idle`, `Busy`, and `Searching` state that can drive later visual/command behavior.
- **BREAKING** Change catalog mutations such as Add/Update Disk Image from immediate database commits to staged pending edits; the real active catalog database is changed only when the user invokes Save.
- Implement dirty-state handling and functional Save behavior: successful Save persists all pending catalog edits and returns status to `Loaded`, while failure leaves pending data and `Modified` state intact.
- Protect unsaved work when switching catalogs or exiting through a Save/Discard/Cancel decision when no equivalent prompt is already active.

## Capabilities

### New Capabilities

- `main-status-bar`: Defines the five-section native status bar, focused/selected item summaries, catalog protection indicator, and extensible program-status lights.
- `catalog-deferred-save`: Defines in-memory/pending catalog edits, dirty state, explicit Save persistence, failed-save retention, and unsaved-change handling on switch or exit.

### Modified Capabilities

## Impact

- Main-frame status control creation, resizing, notifications, command routing, scan completion, catalog activation, and shutdown orchestration in `src/app`.
- Owner-data contents-list focus/selection observation in `src/ui`, reuse of size formatting in `src/core`, and potentially small icon/resource additions in `src/app`.
- Storage/scanning boundaries in `src/storage` and `src/core` so scan output can be staged and later committed atomically to the active SQLite catalog file.
- Existing implemented Add/Update behavior from `catalog-lifecycle-and-startup`: its immediate successful scan commit is superseded for pending user edits by explicit Save.
- Canonical UI, storage, architecture, and acceptance documentation plus Release x64 and focused workflow verification.
