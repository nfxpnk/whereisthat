## MODIFIED Requirements

### Requirement: Hierarchical folder tree
The system SHALL show indexed source roots and stored descendant folders beneath the active catalog root in the left tree, SHALL reveal child folders when a folder is expanded, SHALL NOT display indexed files in the tree, and SHALL display an expand toggle for a folder node only when that folder has at least one stored child folder.

#### Scenario: Catalog root is expanded
- **WHEN** the user expands the active catalog root node
- **THEN** each indexed source root stored in that catalog is available as a folder child node

#### Scenario: Folder node is expanded
- **WHEN** the user expands a source or descendant folder node that has stored child folders
- **THEN** its immediately contained stored folders are displayed beneath that node
- **THEN** no file item is displayed as a tree node

#### Scenario: Stored folder has child folders
- **WHEN** a source or descendant folder has at least one immediately contained stored folder
- **THEN** its node in the folder tree displays an expand toggle

#### Scenario: Stored folder is a leaf in the tree
- **WHEN** a source or descendant folder has no immediately contained stored folders
- **THEN** its node in the folder tree does not display an expand toggle
- **THEN** the folder remains selectable so its stored contents can be browsed
