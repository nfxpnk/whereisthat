## ADDED Requirements

### Requirement: Multiple open catalog database workspace
The system SHALL allow more than one supported catalog database file to remain open at the same time, SHALL represent each open file as one separate top-level catalog item in the TreeView, and SHALL scope each catalog item's descendants to data read from that database's current working view.

#### Scenario: User opens multiple catalogs
- **WHEN** the user successfully creates or opens a catalog database while another catalog is open
- **THEN** the previously open catalog remains open and unchanged
- **THEN** the TreeView displays a top-level item for each open catalog database

#### Scenario: User browses catalog descendants
- **WHEN** the user expands or selects a disk or descendant folder below an open catalog top-level item
- **THEN** displayed child locations and right-pane contents are queried from that owning catalog only
- **THEN** contents from another open database are not substituted into that location

#### Scenario: User opens an already open database path
- **WHEN** the user attempts to open a database file that is already open in the workspace through an equivalent Windows filesystem path
- **THEN** the system selects or activates the existing top-level catalog item
- **THEN** the system does not create a duplicate open connection or duplicate top-level item

#### Scenario: Startup restores a previous catalog
- **WHEN** startup restoration successfully opens the last-used catalog path
- **THEN** that catalog is initially represented as an open top-level TreeView item
- **THEN** the system is not required to reopen every catalog that may have been open before the previous application shutdown

### Requirement: Active catalog follows tree context
The system SHALL define the active catalog for catalog-scoped menu, toolbar, status, search, save, and default edit operations as the owning top-level catalog of the current TreeView selection.

#### Scenario: Catalog root is selected
- **WHEN** the user selects a top-level catalog item in the TreeView
- **THEN** that catalog becomes the active catalog for catalog-scoped commands and status display

#### Scenario: Catalog child is selected
- **WHEN** the user selects a disk or folder item nested under a top-level catalog item
- **THEN** its owning top-level catalog becomes the active catalog
- **THEN** operations do not mistakenly target another top-level catalog

#### Scenario: Selected catalog is removed
- **WHEN** the active catalog closes and at least one other catalog remains open
- **THEN** the system selects a remaining catalog root and establishes it as active
- **THEN** catalog-dependent UI is refreshed for the newly active catalog

### Requirement: Independent per-catalog pending state
The system SHALL maintain editable/protected state, pending modifications, Save, and discard behavior independently for each open catalog database.

#### Scenario: One catalog is modified
- **WHEN** a successful edit is staged for one open catalog
- **THEN** that catalog is marked modified and presents its staged working contents
- **THEN** other open catalogs do not become modified or receive the staged contents

#### Scenario: Active modified catalog is saved
- **WHEN** the user invokes Save while a modified editable catalog is active and saving succeeds
- **THEN** only that active catalog's pending changes are committed and cleared
- **THEN** pending changes in any other open catalog remain pending

#### Scenario: Save of a selected catalog fails
- **WHEN** Save is requested for an active modified catalog and persistence fails
- **THEN** that catalog remains open with pending changes intact
- **THEN** no other open catalog is closed, discarded, or modified by the failed save

### Requirement: Close selected catalog from File menu
The system SHALL implement `File > Close` to close the active catalog resolved from the current TreeView selection and SHALL disable that command when no catalog is open.

#### Scenario: Child item determines File Close target
- **WHEN** the user selects a disk or descendant folder below a catalog root and invokes `File > Close`
- **THEN** the close workflow targets that item's owning top-level catalog only

#### Scenario: No catalogs remain
- **WHEN** the last open catalog is closed successfully
- **THEN** its top-level TreeView item and displayed contents are removed
- **THEN** `File > Close` is disabled and the TreeView is empty

### Requirement: Close action on catalog-root context menu
The system SHALL make `Close Catalog` available from the TreeView context menu only for an opened catalog's top-level item and SHALL use the same targeted close workflow as `File > Close`.

#### Scenario: Root item context menu is opened
- **WHEN** the user right-clicks a top-level catalog TreeView item
- **THEN** that item becomes selected
- **THEN** its context menu offers `Close Catalog`

#### Scenario: Child item context menu is opened
- **WHEN** the user right-clicks a disk, folder, or file-related child location below a catalog root
- **THEN** `Close Catalog` is not actionable for that child context because it is absent or disabled

#### Scenario: Context close is selected
- **WHEN** the user chooses `Close Catalog` from a top-level catalog item's context menu and completes the close workflow
- **THEN** only that selected catalog is removed from the open workspace

### Requirement: Confirm and resolve changes before catalog close
The system SHALL ask `Are you sure you want to close this catalog?` before closing a selected catalog with `No` as the safe default, and SHALL resolve that selected catalog's unsaved changes before removal.

#### Scenario: User declines close confirmation
- **WHEN** the close confirmation is shown and the user chooses `No` or activates the default safer action
- **THEN** the selected catalog remains open without discarding or saving pending changes

#### Scenario: Clean catalog close is confirmed
- **WHEN** the selected catalog has no pending changes and the user chooses `Yes` at the close confirmation
- **THEN** its database handle is closed and its top-level TreeView item and session state are removed
- **THEN** all other open catalogs remain open

#### Scenario: Modified catalog requests unsaved decision
- **WHEN** the user confirms closing a catalog that has pending changes
- **THEN** the system displays an additional `Save changes?` decision with `No`, `Cancel`, and `Yes` because Save is supported

#### Scenario: User cancels unsaved close
- **WHEN** the user chooses `Cancel` from the `Save changes?` decision
- **THEN** closing is aborted and the selected modified catalog remains open with pending contents intact

#### Scenario: User discards on close
- **WHEN** the user chooses `No` from the `Save changes?` decision
- **THEN** the selected catalog closes without committing its pending changes
- **THEN** pending or saved state of other catalogs is unchanged

#### Scenario: User saves on close
- **WHEN** the user chooses `Yes` from the `Save changes?` decision and Save succeeds
- **THEN** the selected catalog's pending changes are committed before that catalog closes

#### Scenario: Save before close fails
- **WHEN** the user chooses `Yes` from the `Save changes?` decision and Save fails
- **THEN** an error is reported and the selected catalog remains open with its pending changes available

### Requirement: Close lifetime safety and empty workspace
The system SHALL remove a closed catalog's tree/browser state, release its database connection, and reject closing a catalog while background work still targets its pending state.

#### Scenario: Unrelated catalog is closed while scan runs
- **WHEN** a scan targets one open catalog and the user closes a different eligible catalog
- **THEN** the unrelated catalog may close without invalidating the running scan target

#### Scenario: Target catalog close is requested while scan runs
- **WHEN** a scan is in progress for the selected catalog and the user requests close for that same catalog
- **THEN** the system does not remove that catalog until its scan is safely completed or cancelled and retired

#### Scenario: Closed history target is removed
- **WHEN** a catalog is closed while browser navigation history contains locations within it
- **THEN** navigation no longer attempts to use the closed catalog's database state
