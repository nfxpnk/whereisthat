## Context

The main frame menu is currently defined in `src/app/app.rc` with `File > Scan Folder or Disk...`, `Refresh`, and `Exit`; `MainFrame::OnNewCatalog()` handles that scan action by selecting a filesystem root, deriving a catalog name from the root, inserting the catalog in `catalog.db`, and starting a background scan. The new product menu separates catalog lifecycle commands in File from disk/image acquisition commands in Edit. The application must remain a Unicode Win32/MSVC application, with dialog presentation in the app/UI boundary and persisted catalog data managed through `src/storage`.

## Goals / Non-Goals

**Goals:**
- Expose the requested File and Edit menus in the requested order with visible shortcut labels.
- Keep the existing scan entry point usable under the renamed `Edit > Add/Update Disk Image` command.
- Implement `File > New Catalog` as a modal native name prompt that persists and selects an empty named catalog.
- Preserve the single `catalog.db` persistence model, background scan behavior, and paged browsing behavior.

**Non-Goals:**
- Implement Open, Save, Save As, Rebuild Catalog Database, Close, Catalog Info, Report Generator, Catalog Manager, or Catalog Setup behavior.
- Define how an empty catalog later receives one or more disk images or how an existing scanned catalog is updated.
- Add a database-file chooser, change the schema, or introduce a new dependency or UI framework.

## Decisions

- Define the File/Edit layout in the existing Win32 `MENU` resource and allocate distinct command IDs for new items. Menu label text will append tab-separated accelerator presentation (`\tCtrl+N`, `\tCtrl+O`, `\tCtrl+S`, `\tCtrl+D`, and `\tAlt+F4`) so shortcuts are displayed in the native menu. Alternative considered: manually draw menu rows, rejected because standard resources already provide correct native presentation.
- Add an `ACCELERATORS` resource only for functional commands introduced or retained in this step: `Ctrl+N` maps to New Catalog and `Ctrl+D` maps to Add/Update Disk Image. `Alt+F4` remains the standard top-level window close shortcut. Placeholder labels such as `Ctrl+O` and `Ctrl+S` remain display-only until those commands are implemented, avoiding commands that misleadingly appear to succeed. Alternative considered: bind every displayed shortcut to no-op handlers, rejected because it adds no useful behavior.
- Move the existing folder/disk scan handler to a renamed `ID_EDIT_ADDDISKIMAGE` route while keeping its folder-selection, insert-and-scan, background progress, and completion path unchanged. Alternative considered: repurpose New Catalog to immediately run a scan, rejected because the requested New Catalog workflow is name creation while Edit is explicitly assigned the former scan command.
- Implement New Catalog with a modal Unicode Win32 dialog containing a label, edit field, OK, and Cancel controls. Confirmation requires a nonblank trimmed name; invalid confirmation keeps the dialog open and explains that a catalog name is required. Cancel closes without changing persistent data. Alternative considered: prompt through a third-party or managed dialog facility, rejected by the native UI constraints.
- Persist a confirmed New Catalog entry through storage-layer catalog insertion using the entered name, creation timestamp, empty root path, and zero item count, then reload and select it in the catalog list. The existing schema accepts an empty `root_path` value and no migration is required. Alternative considered: create a separate database per catalog, rejected because the canonical storage contract specifies the local `catalog.db` database.
- Apply the existing scan-in-progress protection to New Catalog while a background scan is active. Both workflows write to `catalog.db`, and deferring a new empty-catalog insert avoids adding SQLite write contention or failure handling to this menu-focused change. Alternative considered: permit concurrent insertion, rejected because the current storage API does not surface a busy/failed insert reliably to the UI.
- Leave the menu placeholders inert in command routing until later proposals implement their user workflows; retain existing Exit functionality. This keeps the command surface visible without silently inventing storage or document behavior.

## Risks / Trade-offs

- Empty catalogs have no root path or file rows until a future image-association workflow is implemented -> display them as selectable empty entries and explicitly leave scan-to-existing-catalog behavior for a subsequent change.
- Displayed `Ctrl+O` and `Ctrl+S` hints are initially not actionable because their menu entries are placeholders -> keep their routing inert and cover functional shortcuts separately in verification.
- Renaming and moving the scan command could accidentally disrupt existing scanning -> route the new Edit command ID to the established asynchronous handler and smoke test progress/completion.
- A user cannot create another catalog during a running scan -> preserve data-write reliability now and revisit concurrent catalog operations with a future storage workflow.
- New Catalog could save blank or whitespace-only names -> trim/validate before storage insertion and leave the dialog active on invalid input.

## Migration Plan

No database migration is required. Implement the resource IDs/menu/dialog/accelerators, update main-frame routing and catalog creation behavior, then build and smoke test against existing `catalog.db` data. Reverting the resource and routing changes restores the prior command surface; newly created empty catalog rows remain ordinary persisted user data.

## Open Questions

- A later change must specify whether `Add/Update Disk Image` attaches a scan to the selected empty catalog, supports multiple images per catalog, or continues creating scan-derived catalogs.
