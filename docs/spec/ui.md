# User Interface Specification

## UI Platform

The user interface is a native Unicode Win32 desktop interface written with the pure Win32 API and Windows Common Controls. It must not use .NET, WPF, C#, Qt, Electron, or WTL.

## Primary Experience

The app must provide a direct desktop workflow:

- A main frame window for catalog browsing.
- Commands to create a new catalog database file or open an existing catalog database file.
- A command to choose a folder or disk image for adding to or updating in the active catalog.
- A media-source view for selecting prior scans in the active catalog.
- A file view showing metadata from the selected indexed source.
- Scan progress and completion state in the status bar when that bar is enabled.

The active catalog remains useful when its indexed original storage is disconnected; the UI displays persisted data rather than relying on live filesystem access for browsing. When no database is active, the UI displays an empty catalog state and permits New Catalog and Open.

## Main Menu Workflow

- The File menu exposes catalog lifecycle commands: New Catalog, Open, Open Recent, Save, Save As, Rebuild catalog database, Close, Catalog Info, Report Generator, and Exit.
- The Edit menu follows File and exposes Add/Update Disk Image, Catalog Manager, and Catalog Setup.
- The View menu follows Edit and exposes Sort items, View Icons, View List, View Small Icons, View Details, View Thumbnails, Columns Setup, Show Alias Item Names, Locate in Catalog, Toolbar, and Status Bar.
- The Search menu follows View and exposes Search for Items, Find in This Catalog, Find Selected Items, Scan for Duplicates, Compare to Media, Compare Files to Catalog, and Compare Cataloged Data.
- The Actions menu follows Search and exposes Open in Explorer, View File, Launch File, Edit Description, User List, Rename Catalog, Remove Alias Name, File Management, Remove from Catalog, Remove Archive Contents, Remove Thumbnail, and Properties.
- The Options menu follows Actions and exposes General Settings, User Interface Setup, File List Settings, Disk Image Settings, and Description Settings.
- New Catalog prompts for a new database file destination, creates a fresh empty SQLite catalog at that path without overwriting an existing file, switches the active catalog, and stores its path in settings.
- Open prompts for an existing SQLite catalog database, switches the active catalog after a successful open, and stores its path in settings.
- Open Recent lists up to ten successfully activated catalog database paths in most-recent-first order and uses the same validated activation behavior as Open.
- On startup the available last-used catalog path from settings is reopened; when it is absent or unavailable the application remains empty without creating or opening `catalog.db` implicitly.
- Add/Update Disk Image requires an active catalog and scans the chosen folder or disk into that database only; selecting an already indexed source refreshes it without duplicate source contents.
- Search for Items (`Ctrl+F`) requires an active catalog and opens a native search window with a file/folder name input and Search action; matching indexed items from all media sources in that database appear below the controls with name, type, size, path, and modified metadata.
- General Settings opens a native settings dialog; its initial `Show status bar` preference immediately controls status-bar visibility and defaults to enabled, and it displays the stored last-opened catalog path as read-only information.
- Commands still introduced as placeholders, including the remaining Search commands, have no storage, settings, catalog, file, or display effect until their feature behavior is specified and implemented.

## Win32 Interaction Rules

- Window lifetime, message routing, menu commands, and dialogs use Win32 primitives.
- Standard native dialogs should be used for filesystem selection where suitable.
- Common Controls should provide standard list and status presentation.
- Expensive filesystem scanning or database operations triggered by the UI must not block the main window message pump.
- Updates from background activity must be safely marshaled to the UI thread through Win32 messaging or equivalent native synchronization.
- Catalog switching must not occur while an asynchronous scan is mutating the active database.

## Large Result Sets

- Catalog contents must be viable for large scans.
- The file result view uses virtual/owner-data ListView behavior with database-backed paging.
- The Search for Items result list uses virtual/owner-data ListView behavior with database-backed paging.
- UI changes must not require loading all rows in a catalog into memory merely to display them.

## Consistency Rule

New user-facing views or controls should extend the existing native desktop design and module ownership: presentation in `src/ui`, frame/orchestration in `src/app`, and persisted queries in `src/storage`.
