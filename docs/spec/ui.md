# User Interface Specification

## UI Platform

The user interface is a native Unicode Win32 desktop interface written with the pure Win32 API and Windows Common Controls. It must not use .NET, WPF, C#, Qt, Electron, or WTL.

## Primary Experience

The app must provide a direct desktop workflow:

- A main frame window for catalog browsing.
- A command to choose a folder or disk for scanning.
- A catalog view for selecting prior scans.
- A file view showing metadata from the selected catalog.
- Visible scan progress and completion state.

The catalog remains useful when its original storage is disconnected; the UI displays persisted data rather than relying on live filesystem access for browsing.

## Win32 Interaction Rules

- Window lifetime, message routing, menu commands, and dialogs use Win32 primitives.
- Standard native dialogs should be used for filesystem selection where suitable.
- Common Controls should provide standard list and status presentation.
- Expensive filesystem scanning or database operations triggered by the UI must not block the main window message pump.
- Updates from background activity must be safely marshaled to the UI thread through Win32 messaging or equivalent native synchronization.

## Large Result Sets

- Catalog contents must be viable for large scans.
- The file result view uses virtual/owner-data ListView behavior with database-backed paging.
- UI changes must not require loading all rows in a catalog into memory merely to display them.

## Consistency Rule

New user-facing views or controls should extend the existing native desktop design and module ownership: presentation in `src/ui`, frame/orchestration in `src/app`, and persisted queries in `src/storage`.
