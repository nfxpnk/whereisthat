## ADDED Requirements

### Requirement: Persisted folder content totals
The system SHALL persist for every stored folder a recursive content size in bytes equal to the sum of stored file sizes owned by that folder and all of its stored descendant folders, and SHALL derive that value as part of a successful disk scan rather than while browsing.

#### Scenario: Folder containing nested files is scanned
- **WHEN** a successful scan stores a folder with files directly inside it and files inside stored descendant folders
- **THEN** that folder record stores the combined byte size of all those contained stored files
- **THEN** each descendant folder record stores the corresponding total for its own stored subtree

#### Scenario: Empty folder is scanned
- **WHEN** a successful scan stores a folder whose stored subtree contains no file
- **THEN** its persisted recursive content size is `0`

#### Scenario: Disk contents are rescanned
- **WHEN** a successful rescan replaces stored content for an existing disk
- **THEN** the replacement folder rows contain content totals derived from the new scan result
- **THEN** obsolete folder totals are removed with the obsolete folder contents

#### Scenario: Scan is cancelled or fails
- **WHEN** a staged scan is cancelled or fails before completion
- **THEN** incomplete folder rows or partial content totals do not become saved catalog content

### Requirement: Required folder-size catalog format
The system SHALL require persisted folder content size data as part of its supported normalized catalog format and SHALL NOT migrate or backfill catalogs that lack the required folder-size field.

#### Scenario: Fresh catalog is created
- **WHEN** the application creates a new catalog database
- **THEN** its normalized `folders` storage includes a required non-null recursive content size field

#### Scenario: Earlier normalized catalog is opened
- **WHEN** the user selects a catalog database that otherwise resembles the normalized catalog format but lacks the required folder content size field
- **THEN** the application rejects it as unsupported for this application version
- **THEN** the application does not alter, migrate, or backfill that database

### Requirement: Stored folder sizes in right-pane browsing
The system SHALL show persisted folder content sizes in the right-pane contents view and SHALL obtain folder and file size values from stored rows without computing recursive descendant totals as part of page display.

#### Scenario: Folder and file rows are displayed
- **WHEN** a selected stored location contains immediate folder and file rows
- **THEN** the `Size` column displays each file's persisted byte size and each folder's persisted recursive content size using the application size format

#### Scenario: Folder row contributes to status
- **WHEN** a folder row is focused or included among selected right-pane contents rows
- **THEN** focused or selected-size status reporting uses that folder's persisted recursive content size

#### Scenario: Source media is disconnected
- **WHEN** a user browses persisted folder rows while the originally indexed source is unavailable
- **THEN** stored folder sizes remain displayable without live filesystem access

#### Scenario: Contents page is requested
- **WHEN** the owner-data contents list requests a database-backed page for a selected disk or folder
- **THEN** storage projects the persisted folder content-size field and persisted file size field for displayed rows
- **THEN** page display does not execute a recursive folder-size aggregation over descendant file rows

### Requirement: Indexed immediate-content paging foundation
The system SHALL obtain a selected stored location's immediate child folder and file rows through persisted folder relationships and indexed database access, and SHALL preserve authoritative database ordering and paging rather than sorting an incomplete UI page in memory.

#### Scenario: Selected location contains immediate items
- **WHEN** the right-pane contents list requests the count or a page of items for a selected disk or descendant folder location
- **THEN** storage resolves and retrieves immediate stored children using folder ownership relationships and suitable indexes
- **THEN** the UI does not materialize an entire catalog or disk solely to display that location

#### Scenario: Stable size data is available for later sorting
- **WHEN** a later database-backed sort implementation orders folder and file content rows by size
- **THEN** folder rows have persisted numeric content-size values and file rows have persisted numeric file-size values available to the storage query
- **THEN** this capability does not require an in-memory folder-size overlay to establish size ordering
