## MODIFIED Requirements

### Requirement: Location contents browsing
The system SHALL display persisted indexed disks in a disk inventory list when the selected catalog location is the catalog root, SHALL display folders and files immediately contained by a selected disk or stored descendant folder using stored catalog metadata, and SHALL navigate into a displayed disk or folder when the user activates it.

#### Scenario: Catalog root is selected
- **WHEN** the user selects the active catalog root node in the left tree
- **THEN** the right pane displays one disk inventory row for each indexed disk in that catalog
- **THEN** the right pane does not present those disks as generic file or folder content rows

#### Scenario: Disk row is activated in root inventory
- **WHEN** the user activates a disk row displayed for the active catalog root
- **THEN** that disk's stored root location becomes the current location
- **THEN** the contents list, tree selection, and address display reflect the disk location

#### Scenario: Stored disk or folder is selected in the tree
- **WHEN** the user selects a disk root or descendant folder in the left tree
- **THEN** the contents list displays only files and folders immediately contained by that stored location
- **THEN** display does not depend on the original source media being connected

#### Scenario: Folder is activated in contents
- **WHEN** the user activates a folder shown for a disk or descendant-folder location in the right contents list
- **THEN** that folder becomes the current location
- **THEN** the contents list, tree selection, and address display reflect the new location

#### Scenario: Location contains many items
- **WHEN** the catalog root contains many indexed disks or a selected disk/folder contains many immediate items
- **THEN** the right-pane list retrieves the current location's stored rows in database-backed pages rather than materializing the entire result set in UI memory

## ADDED Requirements

### Requirement: Catalog root disks view columns
The system SHALL present the catalog-root disk inventory in a dedicated details view with columns in the following order: `Disk Name`, `Media Type`, `Capacity`, `Free Space`, `Last Updated`, `Disk #`, `Description`, `Category`, and `Disk Location`.

#### Scenario: Root disks view is shown
- **WHEN** the active catalog root is current
- **THEN** the right pane displays the disk inventory columns in the specified order
- **THEN** each disk row populates those columns from the corresponding persisted disk metadata

#### Scenario: User navigates between root and disk contents
- **WHEN** navigation changes from the active catalog root to a disk or descendant-folder location
- **THEN** the right pane displays the file/folder contents columns appropriate for that location
- **WHEN** navigation returns to the active catalog root
- **THEN** the right pane restores the disk inventory columns and rows

#### Scenario: Indexed media is unavailable
- **WHEN** a disk's original media or filesystem source is disconnected while its catalog remains open
- **THEN** its catalog-root disk row and stored disk metadata remain displayable in the disks view
