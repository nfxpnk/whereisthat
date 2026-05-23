## ADDED Requirements

### Requirement: Last-used catalog setting
The system SHALL persist the path of the last successfully created or opened active catalog database in application settings separate from catalog database content.

#### Scenario: User creates a catalog
- **WHEN** `File > New Catalog` successfully creates and activates a catalog database
- **THEN** the application stores that database path as the last-used catalog path in `settings.ini`

#### Scenario: User opens a catalog
- **WHEN** `File > Open` successfully activates an existing catalog database
- **THEN** the application stores that database path as the last-used catalog path in `settings.ini`

#### Scenario: Catalog activation fails
- **WHEN** a requested create or open operation fails before activating a catalog
- **THEN** the application does not replace the stored last-used catalog path with the failed path

### Requirement: Startup restoration of the active catalog
The system SHALL attempt to activate the saved last-used catalog database when the application starts and SHALL otherwise begin with no active catalog.

#### Scenario: Saved catalog remains available
- **WHEN** the application starts with a saved last-used catalog path that identifies an available usable catalog database
- **THEN** that database is automatically opened as the active catalog
- **THEN** the application's catalog views are populated from that active database

#### Scenario: No catalog has previously been used
- **WHEN** the application starts without a stored last-used catalog path
- **THEN** it displays the empty state with no active catalog database loaded
- **THEN** it does not create or implicitly open a file named `catalog.db`

#### Scenario: Saved catalog is unavailable
- **WHEN** the application starts with a stored last-used catalog path whose database is missing, inaccessible, or unusable
- **THEN** it displays the empty state with no active catalog database loaded
- **THEN** it does not create, overwrite, or modify a replacement database at that path
