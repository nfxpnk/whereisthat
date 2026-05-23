## 1. Native Menu And Dialog Resources

- [x] 1.1 Add resource identifiers for the new File/Edit commands, catalog-name dialog controls, and accelerator table in `src/app/resource.h`.
- [x] 1.2 Replace the File resource layout and insert the Edit menu in `src/app/app.rc`, preserving the required order and visible shortcut text while leaving unrelated Search and Help menus intact.
- [x] 1.3 Define the native New Catalog name-entry dialog and functional `Ctrl+N`/`Ctrl+D` accelerator mappings in the resource script.

## 2. Command Routing And Catalog Creation

- [x] 2.1 Load and translate the accelerator table from the application message loop so functional shortcuts reach the main frame.
- [x] 2.2 Move the existing asynchronous folder/disk scan command route to `Edit > Add/Update Disk Image` while preserving source selection, in-progress protection, progress reporting, and completion behavior.
- [x] 2.3 Implement modal New Catalog prompting, including Cancel behavior and validation that prevents empty or whitespace-only names from being accepted.
- [x] 2.4 Persist a confirmed named empty catalog through the existing storage boundary with no scanned root and zero items, then reload and select it in the catalog list without starting a scan.
- [x] 2.5 Leave the remaining new File/Edit placeholder command IDs without catalog/database effects and preserve existing Exit behavior.

## 3. Product Documentation And Verification

- [x] 3.1 Update `docs/spec/ui.md` as needed to record the main menu and named empty-catalog workflow as part of the native UI contract.
- [x] 3.2 Build `whereisthat.sln` for `Release|x64` with MSBuild and confirm the resource/dialog/accelerator changes compile without adding dependencies.
- [ ] 3.3 Smoke test File/Edit ordering and labels, functional `Ctrl+N` and `Ctrl+D`, native `Alt+F4`, inert placeholders, catalog prompt validation/cancellation/persistence, and unchanged asynchronous scan behavior.
