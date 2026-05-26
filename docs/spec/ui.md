# User Interface Specification

## UI Platform

The user interface is a native Unicode Win32 desktop interface using the Win32 API, Windows Common Controls, and WTL 10.01/ATL native wrappers. The main frame and application message loop are WTL-hosted. It must not use .NET, WPF, C#, Qt, or Electron.

## Primary Experience

The app must provide a direct desktop workflow:

- A main frame window for catalog browsing.
- An Explorer-style main browser with one top-level tree root for each open catalog, folder-only hierarchy below each root, and either selected-root disk inventory or location contents on the right.
- A compact navigation row above the contents list with Back, Forward, and a read-only catalog-relative address display.
- Commands to create a new catalog database file or open an existing catalog database file.
- A command to choose a folder or disk image for adding to or updating in a selected editable open catalog.
- A catalog-root disk inventory view for selecting prior scans in the active catalog determined by TreeView selection.
- A file view showing metadata from the selected indexed source.
- A five-section status bar, when enabled, showing catalog state, protected status, focused-item metadata, selected-item totals, and program status lights.

Each open catalog remains useful when its indexed original storage is disconnected; the UI displays persisted data rather than relying on live filesystem access for browsing. The selected root, or the root owning a selected descendant, is active for scoped commands. When no database is open, the UI displays an empty catalog state and permits New Catalog and Open.

## Main Menu Workflow

- The File menu exposes catalog lifecycle commands: New Catalog, Open, Open Recent, Save, Save As, Save All Catalogs, Import XML, Rebuild Catalog Database, Close, Close All, Catalogs Info, Report Generator, and Exit.
- The Edit menu follows File and exposes Add/Update Disk Image, Catalog Manager, and Catalog Setup.
- The View menu follows Edit and exposes Sort items, View Icons, View List, View Small Icons, View Details, View Thumbnails, Columns Setup, Show Alias Item Names, Locate in Catalog, Toolbar, and Status Bar.
- The Search menu follows View and exposes Search for Items, Find in This Catalog, Find Selected Items, Scan for Duplicates, Compare to Media, Compare Files to Catalog, and Compare Cataloged Data.
- The Actions menu follows Search and exposes Open in Explorer, View File, Launch File, Edit Description, User List, Rename Catalog, Remove Alias Name, File Management, Remove from Catalog, Remove Archive Contents, Remove Thumbnail, and Properties.
- The Options menu follows Actions and exposes General Settings, User Interface Setup, File List Settings, Disk Image Settings, and Description Settings.
- The Window menu follows Options and exposes Next Window (`Ctrl+F6`) and Search Window.
- The Help menu follows Window and exposes Help (`F1`) and About.
- New Catalog prompts for a new database file destination, uses the native overwrite confirmation when a file already exists, creates a fresh empty SQLite catalog at a new or confirmed replacement path only when that path is not already open in the application, adds it as a selected TreeView root without closing other roots, and stores its path in settings.
- Open prompts for an existing SQLite catalog database, adds and selects it after a successful open without closing other roots, or selects its existing root when already open, and stores its path in settings.
- Open Recent lists up to ten successfully activated catalog database paths in most-recent-first order and uses the same validated activation behavior as Open.
- On startup the available last-used catalog path from settings is reopened; when it is absent or unavailable the application remains empty without creating or opening `catalog.db` implicitly.
- Add/Update Disk Image (`Ctrl+D`) requires at least one editable open catalog and opens the modal native `Add New Disk/Media` dialog. The dialog presents available local/removable drives, ISO image and network/computer source selection, numeric disk-number and disk-name inputs, a destination catalog selector immediately after `Disk name:`, disk-image and completion option controls, and action buttons. Calculate CRC codes and Browse inside compressed files are functional for persisted scan data; remaining controls without an implemented storage/scanning contract remain presentation-only.
- The Add New Disk/Media dialog starts with `No drive selected` and a disabled `OK` button, enables confirmation only after a readable selected media source (including a natively mounted/resolved ISO) and editable destination catalog are available, and closes without staging changes on Cancel. A confirmed source stages the background add/update scan in the chosen catalog; selecting an already indexed source stages a replacement without duplicate source contents.
- Save (`Ctrl+S`) commits pending catalog content to the active SQLite file; until Save succeeds, the status bar reports `Modified` and the saved catalog file retains its prior content.
- File > Close closes the active TreeView catalog only after an `Are you sure you want to close this catalog?` Yes/No confirmation whose safe default is No; the context-menu Close action is available only on a top-level catalog root.
- Closing a modified catalog asks `Save changes?` with Yes/No/Cancel, retaining that root on Cancel or save failure. Exiting the application similarly resolves pending changes for all modified open catalogs.
- Search for Items (`Ctrl+F`) requires an active catalog and opens a native search window with a file/folder name input and Search action; matching indexed items from all media sources in that database appear below the controls with name, type, size, path, and modified metadata.
- The main folder tree uses a native TreeView: each top-level catalog-database node displays the 16x16 Fugue `database-sql.png` icon, its stored disk/media source children display `drive.png`, and expanding a disk/media node loads only normalized persisted descendant folders from that root's database. Stored ordinary descendant folder nodes display `folder.png`, while stored archive-backed folder nodes display `vise-drawer.png`.
- When the catalog root is selected, the native owner-data right-pane ListView displays one row per stored disk, preceded by the 16x16 Fugue `drive.png` icon, with columns `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`, in that order; activating a disk navigates into its stored root contents.
- When a disk or descendant folder is selected, the same right-pane ListView displays immediate folders and files with content-oriented columns; folders display persisted recursive content size values captured by scanning. Each content name is preceded by a 16x16 Fugue type icon: `folder.png` for an ordinary folder, `document.png` for a non-archive file, and `vise-drawer.png` for an archive container whether it is retained as a file row or exposed as an archive-backed folder. A readable expanded archive is displayed as an `Archive` folder type and may be activated/navigated like a normal folder, including offline. Activating a folder navigates into it and updates the tree selection, address display, and Back/Forward history.
- The main contents area supports multi-selection so the status bar can report selected item count and aggregate stored size; focused item status includes filename, size, and stored date. `Ctrl+A` selects every displayed file and folder only while a file or folder row in the right-pane contents list has focus, and does not apply to the catalog-root disk inventory.
- An active read-only or otherwise protected catalog remains browseable and shows a compact lock indicator, but does not accept Add/Update or Save mutations.
- General Settings opens a dedicated WTL/ATL-hosted native settings dialog in `src/ui`; its initial `Show status bar` preference immediately controls status-bar visibility and defaults to enabled, and it displays the stored last-opened catalog path as read-only information.
- Commands still introduced as placeholders, including the remaining Search commands, have no storage, settings, catalog, file, or display effect until their feature behavior is specified and implemented.

## Win32 Interaction Rules

- Main-frame lifetime, accelerator translation, and top-level message routing use WTL 10.01 frame/message-loop infrastructure while preserving Win32 message and resource behavior.
- Child-window presentation remains owned by the frame collaborators and may use Win32 primitives or WTL/ATL Common Controls wrappers.
- Standard native dialogs should be used for filesystem selection where suitable.
- Common Controls should provide standard list and status presentation.
- Expensive filesystem scanning or database operations triggered by the UI must not block the main window message pump.
- Updates from background activity must be safely marshaled to the UI thread through Win32 messaging or equivalent native synchronization.
- A running scan remains bound to its selected destination catalog even if the user browses another open root; the destination cannot be saved, discarded, or closed until the scan is safely resolved, and concurrent scans are not started.

## Large Result Sets

- Catalog contents must be viable for large scans.
- The main right-pane view uses virtual/owner-data ListView behavior with database-backed paging for both catalog-root disk inventory and file/folder contents scoped to the current location.
- Right-pane file/folder pages read stored file and folder sizes without recursively aggregating descendants while the list is displayed; page ordering remains owned by the database for later specified sort behavior.
- The Search for Items result list uses virtual/owner-data ListView behavior with database-backed paging.
- UI changes must not require loading all rows in a catalog into memory merely to display them.

## Consistency Rule

New user-facing views or controls should extend the existing native desktop design and module ownership: presentation in `src/ui`, frame/orchestration in `src/app`, and persisted queries in `src/storage`.
