## ADDED Requirements

### Requirement: Opt-in archive scanning
The system SHALL make the existing archive-browsing media option functional and SHALL attempt archive expansion during the normal background scan only when that option is selected.

#### Scenario: Archive browsing is selected
- **WHEN** the user confirms a media scan with `Browse inside compressed files (ZIP, ARJ, RAR, 7Z...)` selected
- **THEN** the accepted scan configuration enables libarchive probing of physical non-directory items encountered during that scan

#### Scenario: Archive browsing is not selected
- **WHEN** the user confirms a media scan without selecting archive browsing
- **THEN** physical archive files are stored according to existing ordinary file behavior
- **THEN** no archive-backed folder rows or archive-content counts are produced for those files

### Requirement: Archive-backed folder representation
The system SHALL store each physical archive that is successfully read through libarchive as one archive-backed row in the existing `folders` hierarchy and SHALL keep that row distinguishable from a normal directory row.

#### Scenario: Readable archive is encountered
- **WHEN** archive browsing is enabled and a physical archive can be completely opened and read through libarchive
- **THEN** its containing folder has one child `folders` row representing that archive with an archive-backed type marker
- **THEN** no ordinary `files` row for that same physical archive container is stored

#### Scenario: Empty readable archive is encountered
- **WHEN** archive browsing is enabled and a readable archive contains no stored member entries
- **THEN** the archive is stored as an archive-backed folder with no child content and a stored content size of `0`

#### Scenario: Folder type is consumed for browsing
- **WHEN** archive-backed and physical-directory folder rows are projected to navigation or presentation logic
- **THEN** both rows support normal folder entry behavior
- **THEN** their stored/projected type data allows the application to distinguish the archive-backed row

### Requirement: Archive member hierarchy and metadata
The system SHALL persist safe readable archive members through the existing `folders` and `files` tables while preserving their archive-relative parent-child hierarchy.

#### Scenario: Archive contains nested folders and files
- **WHEN** a readable archive contains a file at a nested relative member path
- **THEN** the file is stored in `files` below the corresponding stored archive descendant folder hierarchy
- **THEN** navigation into the archive exposes the same immediate-folder and immediate-file relationships used for physical folders

#### Scenario: Archive omits explicit parent directory entries
- **WHEN** a readable archive emits a valid nested file member without explicit entries for one or more parent directories
- **THEN** the scanner creates the required intermediate `folders` rows once beneath the archive-backed folder
- **THEN** the file remains reachable through the correct parent-child path

#### Scenario: Archive member path is unsafe
- **WHEN** an archive presents an absolute, rooted, parent-traversing, or otherwise unrepresentable member path
- **THEN** the scanner does not persist a member outside or ambiguously within the archive-backed hierarchy
- **THEN** the candidate archive is handled as unreadable according to archive fallback behavior

### Requirement: Archive fallback and failure isolation
The system SHALL continue a media scan when an encountered archive is unsupported, encrypted, corrupt, or otherwise unreadable, SHALL log the archive read failure, and SHALL treat that physical item as an ordinary file without leaving virtual archive rows.

#### Scenario: Archive cannot be opened
- **WHEN** archive browsing is enabled and libarchive cannot open an encountered physical item as a readable archive
- **THEN** the scanner records a nonfatal diagnostic identifying the physical item and failure
- **THEN** it stores the item as an ordinary file and continues scanning other source content

#### Scenario: Archive fails while members are read
- **WHEN** archive expansion begins but fails because content is corrupt, encrypted, truncated, unsupported, or unreadable
- **THEN** no archive-backed folder or member rows from that candidate remain in the staged scan
- **THEN** the container is stored as an ordinary file and the surrounding media scan may complete successfully

#### Scenario: Scan is cancelled during archive work
- **WHEN** cancellation is requested while libarchive probing or member reading is in progress
- **THEN** archive work stops cooperatively
- **THEN** the outer staged media scan follows existing cancellation behavior rather than publishing fallback or partial content

### Requirement: Shared archive navigation and stored size behavior
The system SHALL make expanded archives and their contents browseable from persisted catalog data through the same hierarchy, paging, search, and stored recursive-size mechanisms used for ordinary folders.

#### Scenario: User enters a scanned archive
- **WHEN** the user activates a persisted archive-backed folder in the folder tree or contents view
- **THEN** the application navigates into its stored immediate content in the same manner as a physical stored folder
- **THEN** browsing remains available when the original source media is disconnected

#### Scenario: Archive content sizes are persisted
- **WHEN** a readable archive with contained files is successfully scanned
- **THEN** files inside the archive store their uncompressed member sizes in `files`
- **THEN** archive-backed and ancestor folder rows store recursive content sizes that include the stored archive file subtree once

#### Scenario: Archive contents participate in ordinary reads
- **WHEN** browse, search, count, pagination, or later sorting logic queries persisted folder/file content
- **THEN** archive-backed folder and contained member rows use the existing normalized folder/file access paths rather than an archive-specific table or navigation path

### Requirement: Archive scan statistics
The system SHALL retain latest successful scan counts for readable scanned archives, files stored inside scanned archives, and folders stored inside scanned archives, alongside existing per-disk scan statistics.

#### Scenario: Successful scan contains readable archives
- **WHEN** an archive-enabled media scan completes with one or more successfully expanded archives
- **THEN** latest statistics record the number of archive-backed container folders expanded
- **THEN** latest statistics record the number of contained archived file rows and contained archived directory rows produced by that scan

#### Scenario: Synthesized archived directories are counted
- **WHEN** an intermediate directory row is synthesized to represent a valid nested archived file path
- **THEN** that row contributes to the archived folder count
- **THEN** each archive-backed container itself contributes to the scanned archive count rather than the archived folder count

#### Scenario: Unreadable archive falls back to file
- **WHEN** an encountered candidate archive is persisted as an ordinary file after a read failure
- **THEN** it does not contribute to scanned archive, archived file, or archived folder counts

### Requirement: Required archive-aware catalog format
The system SHALL add required archive folder-type and latest-statistics fields to the normalized replacement catalog format and SHALL NOT create a separate archives table or migrate catalogs lacking the required fields.

#### Scenario: Fresh catalog is created
- **WHEN** the application creates a new catalog after archive scanning support is delivered
- **THEN** `folders` contains a required archive-versus-directory type representation
- **THEN** `disk_scan_statistics` contains required archive-count storage fields
- **THEN** no `archives` table is required to store expanded archives

#### Scenario: Earlier normalized catalog is opened
- **WHEN** the user attempts to open a normalized catalog lacking required archive folder-type or archive-statistics fields
- **THEN** the application rejects it as unsupported for this application version
- **THEN** the application does not alter, migrate, or backfill that database

### Requirement: Archive verification coverage
The system SHALL verify archive scanning behavior through automated coverage integrated with the existing storage/scan smoke path.

#### Scenario: Readable archive fixtures are tested
- **WHEN** automated storage/scan verification runs with readable archive fixtures, including nested member paths
- **THEN** it verifies archive-backed folder type, hierarchy, member storage, stored sizes, navigation queries, and archive statistics

#### Scenario: Failure and compatibility fixtures are tested
- **WHEN** automated verification runs with archive browsing disabled, unreadable archive input, and an earlier catalog lacking required archive fields
- **THEN** it verifies ordinary-file fallback or unchanged disabled behavior without virtual folders, successful scan continuation, zero failed-archive statistics contribution, and rejection without migration
