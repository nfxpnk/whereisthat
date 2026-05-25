> Storage evolution note: `redesign-catalog-storage-schema` preserves staged Save/Discard behavior while replacing persisted table details with a new-format-only normalized catalog schema.

## Context

`MainFrame` currently creates one native status-bar control but writes only part zero with transient catalog/scan text. The main contents view is an owner-data `ListView` bound to paged `Database` reads and is currently created with single-selection behavior. `wit::core::FormatSize` already formats file sizes, and the toolbar already embeds 16-pixel PNG resources through WIC.

Each active SQLite file is a user catalog containing media-source rows and their indexed file rows. `OnAddOrUpdateDiskImage()` currently launches a worker that opens the real active database path, replaces or adds the source within a SQLite transaction, scans directly into it, and commits on scan completion. `ID_FILE_SAVE` exists as a menu/accelerator/toolbar command but does not yet persist staged work. There is no dirty flag, read-only connection state, or unsaved-change guard.

## Goals / Non-Goals

**Goals:**
- Make the bottom status display stable and useful through five separately updated native status sections.
- Track dirty and protection state for the active catalog and make explicit Save the only point at which pending edits modify its on-disk SQLite contents.
- Preserve pending work and visible `Modified` state after save failures, and guard catalog replacement/application exit from accidental loss.
- Retain asynchronous scanning, database-backed browsing, and native Win32/Common Controls style.
- Establish an application-status type and a status-light update boundary without prematurely defining busy/search command-blocking policy.

**Non-Goals:**
- Implement all future edit commands, Save As, Close, Catalog Manager, or description editing.
- Define red/yellow light art or complete busy/search command-disable behavior.
- Add filesystem write-back, live-media operations, catalog schema redesign, or a managed UI framework.
- Store dirty/pending state across application restarts.

## Decisions

- Use the existing native status-bar control with five parts set from layout width calculations in `OnSize()`, keeping narrow fixed-purpose catalog-state, lock, and light areas while allocating flexible space to focused-item and selection text. Add the requested helpers (`UpdateCatalogStatus()`, `UpdateCatalogLockStatus()`, `UpdateFocusedItemStatus()`, `UpdateSelectionSummaryStatus()`, and `UpdateProgramStatusLights()`) and cache last rendered values/icons before calling status-bar update messages. Alternative considered: five child controls laid out above the status bar, rejected because the Common Controls status bar already supports parts and resize behavior.
- Treat the status contract as stable sections rather than overwriting part zero with progress strings. Catalog load, stage, save, and activation refresh the first two sections; contents `LVN_ITEMCHANGED` notifications refresh focused/selected sections; and an `AppStatus` value owned by main-frame/application orchestration refreshes lights when long-running activity begins or ends. Scanning can continue exposing progress through an adjacent UI treatment later, while this task keeps the five required fields coherent. Alternative considered: continue replacing catalog state text during scans, rejected because it hides dirty state precisely during a mutation workflow.
- Use compact text in sections: `Loaded`/`Modified` for catalog state; an icon-only lock part populated when the active connection is protected; focused entry rendering containing name, formatted size when meaningful, and stored modified date; and `Selected item(s): N (total: X)` using `wit::core::FormatSize`. Remove single-selection mode from the contents list so the stated multi-item summary can operate; enumerate selected visible rows through the existing virtual view adapter on selection notification. Alternative considered: report only the currently focused row because the list is presently single-select, rejected because it would not implement the requested selection contract.
- Extend catalog opening with an explicit editable/protected mode. Opening normally should determine whether the selected catalog can be mutated; a supported catalog that can only be opened read-only remains browsable, sets the lock indicator, and rejects staging/Save operations with a clear message. A newly created catalog is editable. Alternative considered: infer protection only when Save fails, rejected because the status indicator must communicate it before the user tries to edit.
- Introduce active-catalog edit state with a dirty flag and a `PendingCatalogChanges` working boundary owned with the active catalog session. Add/Update Disk Image no longer opens the real active path for scan writes: its worker scans into a pending/working store representing replacement data for the chosen media source, and only a successful staged scan installs that source replacement in pending state and marks the session dirty. The working store may use an in-memory SQLite database or similarly encapsulated pending representation so the existing scanner can stream large numbers of prepared inserts without committing to the selected catalog database. Alternative considered: hold an open transaction on the active catalog until the user saves, rejected because it writes against the real file/WAL during staging, holds locks indefinitely, and makes discard/failure semantics fragile.
- Make reads for affected sources resolve pending replacement data while unmodified sources continue resolving persisted data, so the refreshed browser shows what Save would commit. The staging abstraction supplies the merged read view needed by tree/list browsing and later edits, rather than scattering overlay tests through UI code. Discard clears the working changes and reloads persisted browsing. Alternative considered: display stale saved content until Save, rejected because the user could not inspect the newly staged scan represented by `Modified`.
- Implement `File > Save` as an atomic application of all pending source replacements to the active real database inside a new short-lived storage transaction. Successful commit clears pending data and dirty state and reloads/rebinds the saved catalog view; any error rolls back the real transaction and retains the pending representation and `Modified` status for retry or discard. Alternative considered: move or replace the complete catalog database file, rejected because existing connection/lifecycle behavior and unrelated stored sources can be updated transactionally with less file-replacement risk.
- Guard operations that would abandon a dirty session: before New/Open/Open Recent/close-window/Exit activation proceeds, present a minimal Save/Discard/Cancel decision (`Yes` = Save, `No` = Discard, `Cancel` = stay). Save continues the requested operation only after success; Discard removes pending changes; Cancel or failed Save preserves the existing active session. Continue blocking catalog switching while a scan worker is running. Alternative considered: silently discard or save, rejected because both risk user data or unexpected writes.
- Introduce an `AppStatus` enum (`Idle`, `Busy`, `Searching`) and a single update entry point for the program-lights part. For this task the part displays two placeholder lights/icons, grey and green, while state transitions are wired where existing scanning/search behavior provides meaningful hooks; later work can map red/yellow/green rendering and command enablement without changing status-bar ownership. Alternative considered: hardcode two static bitmaps into `OnCreate`, rejected because it gives future application status no controlled update path.

## Risks / Trade-offs

- [Large staged scans consume working storage or memory] -> Keep pending data behind a storage-like interface that supports streaming inserts and permits a scalable pending backend without changing status or Save contracts.
- [Overlay reads can diverge from persisted-query behavior] -> Centralize pending-versus-persisted resolution in the catalog/session data boundary and cover source replacement, browsing, and discard/save refresh behavior together.
- [A catalog becomes non-writable after staging but before Save] -> Attempt Save atomically, leave pending work intact on failure, retain `Modified`, and report that persistence did not complete.
- [Owner-data selection totals may trigger extra page loads] -> Sum selected indexes only on selection notifications and reuse page/query behavior; avoid full-catalog enumeration.
- [Concurrent scanning and lifecycle actions could lose pending output or mutate the wrong catalog] -> Capture session identity for workers, keep switch/exit protection active while work is running, and accept pending results only for the active session.

## Migration Plan

1. Add the new status-bar and deferred-save requirements to canonical UI, storage, architecture, and acceptance documentation.
2. Refactor active-catalog/session state and storage/scanner integration so mutations produce pending replacements and Save applies them atomically.
3. Add read-only/protected activation behavior, dirty/unsaved prompts, and Save command routing.
4. Split the native status bar, add required update methods/resources/status state, and wire catalog and contents-list notifications.
5. Build and smoke test editable and protected catalogs, staging/save/discard/failure paths, selection/focus updates, status layout/resizing, and catalog switching/exit prompts.

No on-disk schema migration is required. Existing catalog files remain readable; before Save, the new behavior intentionally leaves them byte-for-data unchanged apart from ordinary non-content SQLite open behavior. Rolling back the application does not apply pending in-memory changes, because unsaved pending state is session-local.

## Open Questions

- No decision blocks implementation. If full scans prove too large for an in-memory pending backend, the pending interface can use an isolated temporary SQLite work file while preserving the rule that the selected real catalog is not modified before Save.
