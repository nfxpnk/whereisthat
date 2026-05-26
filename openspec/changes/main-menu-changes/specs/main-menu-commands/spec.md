## ADDED Requirements

### Requirement: File menu command surface
The system SHALL present a File menu containing, in order, `New Catalog`, `Open`, `Save`, `Save As...`, `Save All Catalogs`, `Import XML`, `Rebuild Catalog Database`, `Close`, `Close All`, `Catalogs Info`, `Report Generator`, and `Exit`.

#### Scenario: User opens File menu
- **WHEN** the user opens the File menu in the main window
- **THEN** all required File menu commands are visible in the specified order
- **THEN** `New Catalog` displays `Ctrl+N`, `Open` displays `Ctrl+O`, `Save` displays `Ctrl+S`, and `Exit` displays `Alt+F4` beside their names

#### Scenario: User selects an unimplemented File placeholder
- **WHEN** the user selects `Open`, `Save`, `Save As...`, `Save All Catalogs`, `Import XML`, `Rebuild Catalog Database`, `Close`, `Close All`, `Catalogs Info`, or `Report Generator` in this change
- **THEN** the application performs no catalog or database operation for that placeholder command

#### Scenario: User selects Exit
- **WHEN** the user selects `Exit` or closes the main window with `Alt+F4`
- **THEN** the application closes through its existing exit behavior

### Requirement: Edit menu command surface
The system SHALL present an Edit menu immediately after File containing, in order, `Add/Update Disk Image`, `Catalog Manager`, and `Catalog Setup`.

#### Scenario: User opens Edit menu
- **WHEN** the user opens the Edit menu in the main window
- **THEN** all required Edit menu commands are visible in the specified order
- **THEN** `Add/Update Disk Image` displays `Ctrl+D` beside its name

#### Scenario: User selects Edit placeholders
- **WHEN** the user selects `Catalog Manager` or `Catalog Setup` in this change
- **THEN** the application performs no catalog or database operation for that placeholder command

### Requirement: Named catalog creation
The system SHALL allow the user to create a named empty catalog from `File > New Catalog` and SHALL persist an accepted catalog name in the existing local catalog database.

#### Scenario: User begins new catalog creation
- **WHEN** no scan is active and the user selects `File > New Catalog` or invokes `Ctrl+N`
- **THEN** the application displays a modal native prompt with an input field for the catalog name

#### Scenario: User confirms a valid catalog name
- **WHEN** the user enters a nonblank catalog name and confirms the prompt
- **THEN** the application persists a catalog record with that entered name, zero initial items, and no scanned root assigned
- **THEN** the new catalog becomes available in the main catalog list without initiating a scan

#### Scenario: User cancels catalog creation
- **WHEN** the user cancels the catalog name prompt
- **THEN** the prompt closes without creating or changing a catalog record

#### Scenario: User confirms an invalid catalog name
- **WHEN** the user confirms an empty or whitespace-only catalog name
- **THEN** the application does not persist a new catalog record
- **THEN** the application keeps the prompt available for a valid name and informs the user that a name is required

#### Scenario: User requests a new catalog during scanning
- **WHEN** the user selects `File > New Catalog` or invokes `Ctrl+N` while a scan is active
- **THEN** the application does not open the name prompt or persist a new catalog record
- **THEN** the application informs the user that a scan is already running

### Requirement: Relocated disk scan command
The system SHALL expose the existing folder-or-disk scan workflow through `Edit > Add/Update Disk Image`.

#### Scenario: User starts a disk image scan from Edit
- **WHEN** the user selects `Edit > Add/Update Disk Image` or invokes `Ctrl+D`
- **THEN** the application prompts for a folder or disk to scan using its existing native selection workflow
- **THEN** an accepted source is scanned through the existing background progress and persistence flow

#### Scenario: Existing scan is active
- **WHEN** the user requests `Add/Update Disk Image` while a scan is already active
- **THEN** the application retains its existing scan-in-progress protection behavior
