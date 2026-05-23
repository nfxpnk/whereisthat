## ADDED Requirements

### Requirement: Search for indexed items by name
The system SHALL allow the user to open `Search for Items` from the Search menu or its `Ctrl+F` accelerator and search for a nonblank literal name substring among indexed files and folders in the active catalog database.

#### Scenario: User opens item search
- **WHEN** the user selects `Search for Items` or presses `Ctrl+F` while a catalog database is active
- **THEN** the application displays a native search window with a name input and Search action

#### Scenario: No active catalog is available
- **WHEN** the user invokes `Search for Items` without an active catalog database
- **THEN** the application informs the user that a catalog must be created or opened before searching

#### Scenario: User searches by name
- **WHEN** the user enters a nonblank item-name substring and initiates search
- **THEN** matching indexed files and folders from all media sources in the active catalog database are included by name
- **THEN** characters entered by the user are treated as literal search text rather than SQL wildcard syntax

### Requirement: Search results presentation
The system SHALL display item search matches directly below the search options in the search window with name, type, size, path, and modified metadata, using database-backed paging for the result list.

#### Scenario: Search has matching items
- **WHEN** a search returns one or more matching indexed items
- **THEN** the matches appear in the results list below the input controls
- **THEN** the result count is visible in the search window

#### Scenario: Search has no matching items
- **WHEN** a completed name search finds no indexed item matches
- **THEN** the results list is empty and communicates that no items matched

#### Scenario: Search yields many matching items
- **WHEN** matching rows exceed a single display page
- **THEN** the application retrieves result rows in pages as the owner-data result list requests them rather than loading the full result set into UI memory
