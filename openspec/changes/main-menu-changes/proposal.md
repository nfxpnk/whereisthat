## Why

Where Is That? currently exposes its initial scan command as the primary File action and lacks the application-style catalog commands needed for the next stage of the main window. Establishing the File/Edit menu structure now gives users a recognizable command surface and enables them to create a named catalog before content is added to it.

## What Changes

- Replace the current File menu layout with `New Catalog`, `Open`, `Save`, `Save As...`, `Rebuild catalog database`, `Close`, `Catalog Info`, `Report Generator`, and `Exit`, displaying the requested keyboard shortcuts beside applicable commands.
- Add an Edit menu immediately after File containing `Add/Update Disk Image`, `Catalog Manager`, and `Catalog Setup`, displaying `Ctrl+D` for the first command.
- Relocate the existing `Scan Folder or Disk...` action to `Edit > Add/Update Disk Image` without changing its scan/progress workflow.
- Make `File > New Catalog` (`Ctrl+N`) open a native prompt where the user enters a catalog name, then persist an empty named catalog in the existing catalog database and expose it in the catalog list.
- Treat the other newly listed commands as menu placeholders in this change; `Exit` continues to close the application as it does today.

## Capabilities

### New Capabilities
- `main-menu-commands`: Defines the main frame File/Edit command layout, placeholder behavior, relocated scan entry point, and named empty-catalog creation workflow.

### Modified Capabilities

## Impact

- Affected implementation is expected in Win32 resources and command routing under `src/app`, with native prompt/dialog behavior potentially factored into `src/ui`.
- Existing SQLite catalog persistence in `src/storage` may need a focused way to represent a newly created catalog that has no scanned root or files yet; the database remains `catalog.db`.
- Existing asynchronous scan behavior, paged file browsing, SQLite deployment, and supported MSVC/Win32 architecture remain unchanged.
