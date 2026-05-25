## ADDED Requirements

### Requirement: Replacement catalog schema format
The system SHALL create and use a replacement SQLite catalog format containing normalized catalog metadata, disk, latest disk scan statistics, folder, and file records, and SHALL NOT treat databases using the former `catalogs` and mixed folder/file `files` format as supported catalogs.

#### Scenario: Fresh catalog creation
- **WHEN** the user creates a new catalog database in the updated application
- **THEN** the database is initialized with the replacement catalog tables, relationships, and required indexes
- **THEN** it contains catalog metadata suitable for storing a catalog description and no added disk content

#### Scenario: Old-format catalog selected
- **WHEN** the user attempts to open a database that contains only the former catalog storage format
- **THEN** the application rejects it as an unsupported catalog for this application version
- **THEN** the application does not migrate, alter, or rewrite that database

#### Scenario: New-format catalog saved from staged changes
- **WHEN** staged disk or content changes are explicitly saved
- **THEN** catalog metadata, disks, scan statistics, folders, and files are persisted transactionally with valid foreign-key relationships

### Requirement: Catalog metadata and derived summaries
The system SHALL persist free-text catalog description metadata and SHALL calculate catalog file size and catalog-wide totals without storing derived catalog summary fields in SQLite.

#### Scenario: Catalog description stored
- **WHEN** a catalog description is saved
- **THEN** the free-text description is persisted in the catalog metadata record and can be read back for that catalog

#### Scenario: Catalog summary requested
- **WHEN** catalog summary information is requested for an active catalog
- **THEN** total disks is calculated from persisted disk records
- **THEN** total files is calculated from persisted file records
- **THEN** total folders is calculated from persisted folder records
- **THEN** total storage space is calculated by summing disk total capacity values
- **THEN** total used space is calculated by summing each disk total capacity minus free space

#### Scenario: Catalog file size requested
- **WHEN** the current catalog database file size is requested
- **THEN** the application reads that value from the filesystem for the active database path
- **THEN** the schema contains no persisted catalog file-size field

### Requirement: Disk metadata persistence
The system SHALL persist one disk record for each added media source with disk name, numeric disk number, source identity, volume label, total capacity, free space, cluster size, serial number, file system, per-disk file and folder totals, added and updated timestamps, description, category, location, and disk type.

#### Scenario: New disk is scanned
- **WHEN** a supported source is first successfully scanned into an active catalog
- **THEN** one disk record is stored with its accepted name and numeric disk number
- **THEN** available native volume, capacity, filesystem, location, classification, and descriptive fields are stored
- **THEN** its `added_at` and `updated_at` values are populated as Unix timestamps

#### Scenario: Existing disk is rescanned
- **WHEN** a source already identified in the active catalog is scanned successfully again
- **THEN** its disk content is replaced without introducing another record for the same normalized source identity
- **THEN** its `added_at` value remains its original value and its `updated_at` value reflects the completed rescan

#### Scenario: Disk totals recorded
- **WHEN** a disk scan completes successfully
- **THEN** the disk record stores the total file and total folder counts produced by that completed scan
- **THEN** catalog-wide file and folder summaries remain calculated from normalized file and folder rows

### Requirement: Supported disk types
The system SHALL serialize disk type using only `CD`, `DVD`, `BluRay`, `HardDisk`, `SolidStateDisk`, `RemovableUSB`, `VirtualDisk`, or `Other`.

#### Scenario: Reliably classifiable source
- **WHEN** native metadata or accepted scan input reliably identifies a disk type supported by the catalog schema
- **THEN** the corresponding allowed disk type value is persisted

#### Scenario: Source type cannot be identified reliably
- **WHEN** the application cannot safely distinguish the disk type using available metadata or accepted input
- **THEN** it stores `Other` rather than fabricating a more specific disk type

#### Scenario: Invalid serialized disk type
- **WHEN** a persistence operation attempts to store a disk type outside the supported set
- **THEN** the operation is rejected by domain validation or the database constraint

### Requirement: Latest disk scan statistics
The system SHALL store the latest successful scan statistics for each disk, including last scanned timestamp, elapsed image scanning time in milliseconds, imported description count, and whether file CRC calculation was performed.

#### Scenario: Successful scan records latest statistics
- **WHEN** a disk scan completes successfully
- **THEN** its latest statistics store `last_scanned_at` as a Unix timestamp and `image_scanning_time_ms` as an integer elapsed duration
- **THEN** its imported-description count and CRC-calculation Boolean reflect the completed scan

#### Scenario: Disk is rescanned
- **WHEN** a disk with existing latest scan statistics is successfully rescanned
- **THEN** the latest-statistics record is replaced or updated for that disk rather than appending required scan history

#### Scenario: Scan does not import descriptions
- **WHEN** no implemented description-import source produces descriptions during a completed scan
- **THEN** `imported_descriptions_count` is stored as `0`

### Requirement: Normalized folder storage
The system SHALL persist folders separately from files, associated with their disk and parent folder as needed to count folders and support offline hierarchical browsing.

#### Scenario: Folder discovered during scan
- **WHEN** the scanner enumerates a directory in an added disk
- **THEN** it stores a folder record associated with that disk and its parent hierarchy
- **THEN** native folder metadata retained for browser display is stored using the replacement format

#### Scenario: Folder hierarchy browsed offline
- **WHEN** the user expands or navigates stored folders while the original disk is unavailable
- **THEN** the application resolves child folders and contained files from normalized catalog records without filesystem access to the original disk

#### Scenario: Disk content is refreshed
- **WHEN** a successful disk rescan replaces previously indexed content
- **THEN** obsolete folders and their contained file records for that disk are removed or replaced within the staged transaction

### Requirement: File metadata persistence
The system SHALL persist file-only records with containing disk and folder references, name, free-text description, extension without a leading dot, optional CRC, byte size, created/modified/accessed timestamps, and native attributes.

#### Scenario: File metadata saved
- **WHEN** the scanner successfully enumerates a file
- **THEN** the catalog stores its file metadata and references to the containing disk and folder
- **THEN** `created_at`, `modified_at`, and `accessed_at` are stored as Unix timestamp integers

#### Scenario: File extension normalized
- **WHEN** an enumerated filename has an extension such as `.txt`, `.jpg`, or `.pdf`
- **THEN** the stored extension is `txt`, `jpg`, or `pdf` without the leading dot

#### Scenario: File has no extension
- **WHEN** an enumerated filename has no extension
- **THEN** the stored extension is empty text

#### Scenario: File CRC calculation disabled
- **WHEN** a scan is performed without CRC calculation enabled
- **THEN** file CRC values are stored as null or otherwise explicitly unavailable according to the replacement schema
- **THEN** latest scan statistics record that file CRCs were not calculated

#### Scenario: File CRC calculation enabled
- **WHEN** a scan is performed with CRC calculation enabled and a file can be processed
- **THEN** a consistently formatted CRC-32 text value is stored for that file
- **THEN** latest scan statistics record that CRC calculation was performed

### Requirement: Native file attributes and Boolean storage
The system SHALL store Boolean scan values as SQLite `INTEGER` values using `0` or `1`, and SHALL preserve the native file attribute bitmask sufficient to represent hidden, system, read-only, compressed, and archive attributes.

#### Scenario: File with requested attributes is scanned
- **WHEN** an enumerated file has any combination of the Win32 hidden, system, read-only, compressed, or archive attributes
- **THEN** its stored attributes retain those flag values for later interpretation

#### Scenario: Boolean scan statistic stored
- **WHEN** the application stores whether CRC calculation was performed
- **THEN** it writes `1` for Yes and `0` for No

### Requirement: Timestamp storage format
The system SHALL persist all catalog-format date/time fields as SQLite `INTEGER` Unix timestamps and SHALL format those timestamps for native UI presentation outside the database schema.

#### Scenario: Disk timestamp stored
- **WHEN** a disk is added, updated, or scanned
- **THEN** each recorded disk or scan date is stored as an integer Unix timestamp

#### Scenario: Item timestamp stored
- **WHEN** file or retained folder timestamp metadata is captured from Windows
- **THEN** each persisted timestamp is converted to an integer Unix timestamp

#### Scenario: Timestamp displayed
- **WHEN** the UI displays a persisted date/time value
- **THEN** it formats the stored Unix timestamp for presentation without requiring text-formatted dates in SQLite

### Requirement: Indexed and parameterized storage access
The system SHALL keep SQLite schema and query behavior in the native storage layer, use prepared statements for application or filesystem values, and index implemented disk identity, hierarchy, and file lookup paths required for responsive paging and search.

#### Scenario: Disk is looked up for update
- **WHEN** Add/Update identifies an existing normalized source path
- **THEN** storage performs a parameterized, indexed disk lookup and cannot retain duplicate disk rows for that source identity

#### Scenario: Folder or file list is paged
- **WHEN** the tree or owner-data list requests children for a stored location
- **THEN** storage queries normalized folder/file relationships using suitable indexes and paging without loading the complete catalog into memory

#### Scenario: File extension queried or indexed
- **WHEN** extension-based storage access is implemented or used by maintained browser behavior
- **THEN** it operates on the normalized extension value without a leading dot

### Requirement: Database documentation and validation
The system SHALL maintain database documentation and its lightweight verification tooling to represent the executable replacement schema, its derived values, and its unsupported-old-format contract.

#### Scenario: Replacement schema is implemented
- **WHEN** schema or related persistence code is delivered
- **THEN** `docs/database/` documents every replacement application table and field, relationships, indexes, timestamp and Boolean encodings, disk type values, and derived-versus-stored summaries
- **THEN** affected maintained specifications and architectural documentation no longer promise old-format compatibility

#### Scenario: Documentation verification is run
- **WHEN** the database documentation verifier in `tools/database-docs` is executed
- **THEN** it checks table/field and documented SQL coverage against the new executable schema rather than requiring legacy migration statements
