## ADDED Requirements

### Requirement: Native catalog browser layout
The system SHALL present an active catalog in the main window using a two-column native browser layout with a folder tree in the left column and a contents area in the right column, and SHALL show a navigation bar above the contents area containing Back, Forward, and the current catalog-relative location.

#### Scenario: Active catalog is displayed
- **WHEN** a catalog database is active in the main window
- **THEN** the left column displays a native folder tree whose top-level node represents that active catalog
- **THEN** the right column displays a native contents list beneath Back, Forward, and address controls

#### Scenario: Empty active catalog is displayed
- **WHEN** a newly created or opened active catalog has no indexed source contents
- **THEN** its catalog root remains available in the left tree
- **THEN** the right contents list is empty for that root location

### Requirement: Hierarchical folder tree
The system SHALL show indexed source roots and stored descendant folders beneath the active catalog root in the left tree, SHALL reveal child folders when a folder is expanded, and SHALL NOT display indexed files in the tree.

#### Scenario: Catalog root is expanded
- **WHEN** the user expands the active catalog root node
- **THEN** each indexed source root stored in that catalog is available as a folder child node

#### Scenario: Folder node is expanded
- **WHEN** the user expands a source or descendant folder node that has stored child folders
- **THEN** its immediately contained stored folders are displayed beneath that node
- **THEN** no file item is displayed as a tree node

### Requirement: Location contents browsing
The system SHALL display both folders and files immediately contained by the selected catalog location in the right contents list using stored catalog metadata, and SHALL navigate into a listed folder when the user activates it.

#### Scenario: Catalog root is selected
- **WHEN** the user selects the active catalog root node
- **THEN** the contents list displays its indexed source roots as the first-level catalog contents

#### Scenario: Stored folder is selected in the tree
- **WHEN** the user selects a source or descendant folder in the left tree
- **THEN** the contents list displays only files and folders immediately contained by that stored folder
- **THEN** display does not depend on the original source media being connected

#### Scenario: Folder is activated in contents
- **WHEN** the user activates a folder shown in the right contents list
- **THEN** that folder becomes the current location
- **THEN** the contents list, tree selection, and address display reflect the new location

#### Scenario: Location contains many items
- **WHEN** the current location contains more items than a single display page
- **THEN** the contents list retrieves its immediate stored children in database-backed pages rather than materializing the entire location in UI memory

### Requirement: Catalog-relative navigation
The system SHALL display the current location relative to the active catalog and SHALL provide Back and Forward actions over locations visited during the active browsing session.

#### Scenario: Location display is updated
- **WHEN** the user navigates to a source or descendant folder
- **THEN** the address control displays a catalog-relative location beginning with the active catalog root label and continuing through the browsed folder hierarchy

#### Scenario: Back navigation is used
- **WHEN** the user has visited more than one location and invokes Back
- **THEN** the immediately previous visited location becomes current
- **THEN** the contents list, tree selection, address display, and navigation action states are updated for that location

#### Scenario: Forward navigation is used
- **WHEN** the user has navigated Back and invokes Forward while a later visited location exists
- **THEN** the next visited location becomes current
- **THEN** the contents list, tree selection, address display, and navigation action states are updated for that location

#### Scenario: New navigation discards forward entries
- **WHEN** the user navigates Back and then navigates into or selects a different location
- **THEN** the new location becomes current
- **THEN** locations previously available only through Forward are no longer in the forward history

### Requirement: Browser scope remains read-only and minimal
The system SHALL browse indexed catalog metadata without requiring live filesystem access and SHALL NOT introduce file manipulation or advanced Explorer operations as part of this browser capability.

#### Scenario: Indexed source is unavailable
- **WHEN** the original indexed folder or disk is disconnected while its catalog database remains available
- **THEN** tree and contents navigation continue to show persisted folder and file metadata

#### Scenario: User browses catalog contents
- **WHEN** the user uses the tree, contents list, or navigation bar supplied by this capability
- **THEN** browsing does not copy, rename, delete, move, edit, or launch catalogued filesystem items
