# User Interface Specification

## UI Platform

The user interface is a native Unicode Win32 desktop interface using the Win32 API, Windows Common Controls, and WTL 10.01/ATL native wrappers. The main frame and application message loop are WTL-hosted. It must not use .NET, WPF, C#, Qt, or Electron.

## Primary Experience

The app must provide a direct desktop workflow:

- A main frame window for catalog browsing.
- An Explorer-style main browser with the active catalog as the tree root, folder-only hierarchy on the left, and either root disk inventory or location contents on the right.
- A compact navigation row above the contents list with Back, Forward, and a read-only catalog-relative address display.
- Commands to create a new catalog database file or open an existing catalog database file.
- A command to choose a folder or disk image for adding to or updating in the active catalog.
- A catalog-root disk inventory view for selecting prior scans in the active catalog.
- A file view showing metadata from the selected indexed source.
- A five-section status bar, when enabled, showing catalog state, protected status, focused-item metadata, selected-item totals, and program status lights.

The active catalog remains useful when its indexed original storage is disconnected; the UI displays persisted data rather than relying on live filesystem access for browsing. When no database is active, the UI displays an empty catalog state and permits New Catalog and Open.

## Main Menu Workflow

- The File menu exposes catalog lifecycle commands: New Catalog, Open, Open Recent, Save, Save As, Rebuild Catalog Database, Close, Catalog Info, Report Generator, and Exit.
- The Edit menu follows File and exposes Add/Update Disk Image, Catalog Manager, and Catalog Setup.
- The View menu follows Edit and exposes Sort items, View Icons, View List, View Small Icons, View Details, View Thumbnails, Columns Setup, Show Alias Item Names, Locate in Catalog, Toolbar, and Status Bar.
- The Search menu follows View and exposes Search for Items, Find in This Catalog, Find Selected Items, Scan for Duplicates, Compare to Media, Compare Files to Catalog, and Compare Cataloged Data.
- The Actions menu follows Search and exposes Open in Explorer, View File, Launch File, Edit Description, User List, Rename Catalog, Remove Alias Name, File Management, Remove from Catalog, Remove Archive Contents, Remove Thumbnail, and Properties.
- The Options menu follows Actions and exposes General Settings, User Interface Setup, File List Settings, Disk Image Settings, and Description Settings.
- New Catalog prompts for a new database file destination, creates a fresh empty SQLite catalog at that path without overwriting an existing file, switches the active catalog, and stores its path in settings.
- Open prompts for an existing SQLite catalog database, switches the active catalog after a successful open, and stores its path in settings.
- Open Recent lists up to ten successfully activated catalog database paths in most-recent-first order and uses the same validated activation behavior as Open.
- On startup the available last-used catalog path from settings is reopened; when it is absent or unavailable the application remains empty without creating or opening `catalog.db` implicitly.
- Add/Update Disk Image (`Ctrl+D`) requires an editable active catalog and opens the modal native `Add New Disk/Media` dialog. The dialog presents available local/removable drives, ISO image and network/computer source selection, numeric disk-number and disk-name inputs, disk-image and completion option controls, and action buttons. Calculate CRC codes is functional for persisted file CRC metadata; controls without an implemented storage/scanning contract remain presentation-only.
- The Add New Disk/Media dialog starts with `No drive selected` and a disabled `OK` button, enables confirmation only after a readable selected media source (including a natively mounted/resolved ISO) is available, and closes without staging changes on Cancel. A confirmed source stages the existing background add/update scan path; selecting an already indexed source stages a replacement without duplicate source contents.
- Save (`Ctrl+S`) commits pending catalog content to the active SQLite file; until Save succeeds, the status bar reports `Modified` and the saved catalog file retains its prior content.
- Opening or creating another catalog, or exiting the application, prompts the user to Save, Discard, or Cancel when pending catalog changes exist.
- Search for Items (`Ctrl+F`) requires an active catalog and opens a native search window with a file/folder name input and Search action; matching indexed items from all media sources in that database appear below the controls with name, type, size, path, and modified metadata.
- The main folder tree uses a native TreeView: its top-level node is the active catalog database label, its children are stored disks/media sources, and expanding a node loads only normalized persisted descendant folders.
- When the catalog root is selected, the native owner-data right-pane ListView displays one row per stored disk with columns `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`, in that order; activating a disk navigates into its stored root contents.
- When a disk or descendant folder is selected, the same right-pane ListView displays immediate folders and files with content-oriented columns; activating a folder navigates into it and updates the tree selection, address display, and Back/Forward history.
- The main contents area supports multi-selection so the status bar can report selected item count and aggregate stored size; focused item status includes filename, size, and stored date. `Ctrl+A` selects every displayed file and folder only while a file or folder row in the right-pane contents list has focus, and does not apply to the catalog-root disk inventory.
- An active read-only or otherwise protected catalog remains browseable and shows a compact lock indicator, but does not accept Add/Update or Save mutations.
- General Settings opens a native settings dialog; its initial `Show status bar` preference immediately controls status-bar visibility and defaults to enabled, and it displays the stored last-opened catalog path as read-only information.
- Commands still introduced as placeholders, including the remaining Search commands, have no storage, settings, catalog, file, or display effect until their feature behavior is specified and implemented.

## Win32 Interaction Rules

- Main-frame lifetime, accelerator translation, and top-level message routing use WTL 10.01 frame/message-loop infrastructure while preserving Win32 message and resource behavior.
- Child-window presentation remains owned by the frame collaborators and may use Win32 primitives or WTL/ATL Common Controls wrappers.
- Standard native dialogs should be used for filesystem selection where suitable.
- Common Controls should provide standard list and status presentation.
- Expensive filesystem scanning or database operations triggered by the UI must not block the main window message pump.
- Updates from background activity must be safely marshaled to the UI thread through Win32 messaging or equivalent native synchronization.
- Catalog switching must not occur while an asynchronous scan is preparing pending catalog changes.

## Large Result Sets

- Catalog contents must be viable for large scans.
- The main right-pane view uses virtual/owner-data ListView behavior with database-backed paging for both catalog-root disk inventory and file/folder contents scoped to the current location.
- The Search for Items result list uses virtual/owner-data ListView behavior with database-backed paging.
- UI changes must not require loading all rows in a catalog into memory merely to display them.

## Consistency Rule

New user-facing views or controls should extend the existing native desktop design and module ownership: presentation in `src/ui`, frame/orchestration in `src/app`, and persisted queries in `src/storage`.
