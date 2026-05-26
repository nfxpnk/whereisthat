## ADDED Requirements

### Requirement: Catalog database file identity
The system SHALL treat each opened SQLite catalog database file as a separate catalog containing that file's indexed media-source entries, and SHALL NOT require or implicitly prefer a permanent file named `catalog.db`.

#### Scenario: Arbitrarily named catalog is active
- **WHEN** the user opens or creates a catalog database file whose name is not `catalog.db`
- **THEN** the application uses that SQLite file as the active catalog with the same supported behavior as any file named `catalog.db`

#### Scenario: Multiple catalog files exist
- **WHEN** the user activates one of multiple catalog database files
- **THEN** browsing and catalog mutations are scoped to the activated file only

### Requirement: Create a new catalog database
The system SHALL make `File > New Catalog` prompt for a destination path, create a new empty SQLite catalog database at an unused destination or at a closed existing destination after native overwrite confirmation, refuse any destination that identifies a catalog currently open in the application, and activate the newly created catalog after successful initialization.

#### Scenario: New catalog creation succeeds
- **WHEN** the user chooses an unused destination path in `File > New Catalog` and confirms creation
- **THEN** a new initialized SQLite catalog database is created at that path
- **THEN** the new database becomes the active catalog and initially contains no scanned media source entries

#### Scenario: Another catalog was previously active
- **WHEN** the user successfully creates a new catalog while an existing catalog database was active
- **THEN** the previously active database is not modified by creation of the new database
- **THEN** subsequent browsing and catalog mutations apply to the newly active database

#### Scenario: Closed destination already exists
- **WHEN** the user attempts to create a new catalog at an existing file path that is not open in the application
- **THEN** the native save dialog asks the user to confirm replacement of the existing file
- **THEN** after confirmation the system replaces it with a newly initialized empty catalog and activates that catalog

#### Scenario: Destination is already open
- **WHEN** the user attempts to create a new catalog at the path of a catalog currently open in the application
- **THEN** the system reports that the catalog is currently open and must be closed or a different filename chosen
- **THEN** the system does not overwrite or modify that open catalog
- **THEN** the system does not switch away from the currently active catalog, if any

### Requirement: Open an existing catalog database
The system SHALL make `File > Open` prompt for an existing SQLite catalog database and activate it only after it can be opened as a usable catalog database.

#### Scenario: Existing catalog opens successfully
- **WHEN** the user selects a valid existing catalog database through `File > Open`
- **THEN** that database becomes the active catalog
- **THEN** the views display data loaded from that database rather than data from the formerly active database

#### Scenario: Selected database cannot be opened
- **WHEN** the user selects a missing, unavailable, or unusable catalog database through `File > Open`
- **THEN** the application reports or safely handles the failure without modifying the selected file
- **THEN** any previously active catalog remains active and unchanged

### Requirement: Empty application catalog state
The system SHALL support running with no active catalog database loaded and SHALL allow the user to enter an active-catalog state through New Catalog or Open.

#### Scenario: No catalog is active
- **WHEN** the main window is displayed with no active catalog
- **THEN** no scanned media entries are presented as loaded from an implicit database
- **THEN** `File > New Catalog` and `File > Open` remain available

#### Scenario: Catalog-dependent mutation is requested while empty
- **WHEN** the user invokes `Edit > Add/Update Disk Image` while no catalog is active
- **THEN** the application does not scan or create catalog content
- **THEN** the user is directed to create or open a catalog first

### Requirement: Add or update media in the active catalog
The system SHALL make `Edit > Add/Update Disk Image` add the selected media source contents to the currently active catalog database and refresh existing contents for the same selected source within that database instead of creating duplicates.

#### Scenario: New source is added
- **WHEN** the user selects a media source not yet indexed in the active catalog using `Edit > Add/Update Disk Image`
- **THEN** the scan stores that source and its indexed contents in the active catalog database only

#### Scenario: Previously indexed source is updated
- **WHEN** the user selects a media source already indexed in the active catalog using `Edit > Add/Update Disk Image`
- **THEN** the active catalog contains refreshed contents for that source after the successful operation
- **THEN** the operation does not leave duplicate entries for that same source scan in the active catalog

#### Scenario: Same source exists in another catalog
- **WHEN** the user adds or updates a source while one catalog is active and another catalog database also contains that source
- **THEN** only the active catalog database is modified

#### Scenario: Scan and catalog switching conflict
- **WHEN** a scan/update operation is still running
- **THEN** the application does not switch to or create another active catalog until that operation completes or is otherwise safely ended
