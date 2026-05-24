## 1. Contract And Session State

- [x] 1.1 Update `docs/spec/ui.md`, `docs/spec/storage.md`, `docs/spec/architecture.md`, and `docs/spec/acceptance.md` for five-part status display, protected catalogs, staged mutations, explicit Save, and unsaved-change resolution.
- [x] 1.2 Add active-catalog session state for editable/protected mode, pending/dirty changes, and application activity (`Idle`, `Busy`, `Searching`) without disturbing existing active-path and navigation state.

## 2. Pending Storage And Read Behavior

- [x] 2.1 Extend the storage/open boundary to support readable protected catalogs, report editability to the main frame, and avoid schema/mutation writes when opened read-only.
- [x] 2.2 Implement a pending catalog-change store that can receive streamed Add/Update scan replacements without modifying the selected real catalog database.
- [x] 2.3 Route browser/tree/list reads through a working catalog view so staged source replacements are visible while untouched persisted sources remain browseable.
- [x] 2.4 Implement atomic pending-change application and discard operations, retaining pending content after any failed real-database Save transaction.

## 3. Mutation And Lifecycle Commands

- [x] 3.1 Refactor asynchronous `Add/Update Disk Image` to stage successful scan results for the current editable catalog, mark it modified, and reject or safely report edit attempts for protected catalogs.
- [x] 3.2 Implement `ID_FILE_SAVE` routing shared by the File menu, accelerator, and toolbar so Save persists pending edits, reloads saved views on success, and retains modified state after failure.
- [x] 3.3 Add a minimal Save/Discard/Cancel unsaved-changes guard for New/Open/Open Recent, window close, and Exit, while preserving the existing no-switch-during-scan rule.

## 4. Native Status Bar Surface

- [x] 4.1 Configure five native status-bar parts and resize calculations for catalog state, compact lock indication, focused details, selected summary, and rightmost lights without disrupting toolbar/navigation/content layout.
- [x] 4.2 Add or reuse compact lock and grey/green status-light resources and implement `UpdateCatalogStatus()`, `UpdateCatalogLockStatus()`, and `UpdateProgramStatusLights()` with cached output to avoid unnecessary repainting.
- [x] 4.3 Implement `UpdateFocusedItemStatus()` and `UpdateSelectionSummaryStatus()`, reusing or compactly adapting `wit::core::FormatSize` for status text and enabling multi-selection in the owner-data contents view.

## 5. Status Event Wiring

- [x] 5.1 Refresh catalog/lock status on activation, staged edit, successful/failed Save, discard, and protected-state handling, replacing transient messages that would overwrite required status sections.
- [x] 5.2 Handle contents view focus and selection notifications plus navigation/reload resets so focused details and selected aggregate text never retain stale entries.
- [x] 5.3 Wire activity state updates around existing scan and search entry points and render the required grey/green placeholder lights through the extensible status-light boundary.

## 6. Verification

- [x] 6.1 Build `whereisthat.sln` for `Release|x64` and verify resource, SQLite, and Common Controls integration remains successful.
- [ ] 6.2 Smoke test status-bar ordering/resizing/visibility, catalog `Loaded`/`Modified` transitions, protected lock display, focused-item detail updates, multi-selection totals, and placeholder lights.
- [ ] 6.3 Smoke test that Add/Update leaves the real catalog unchanged before Save, successful Save persists staged replacements, Save failure keeps pending work, and Discard removes only pending changes.
- [ ] 6.4 Smoke test Save/Discard/Cancel behavior during New/Open/Open Recent/Exit and ensure asynchronous scans cannot be applied to or abandon a different active catalog session.
