## ADDED Requirements

### Requirement: Tree-item context menu invocation
The system SHALL display a native context menu only when the user right-clicks a valid item in the catalog browser tree, and SHALL select the clicked item before displaying its context menu.

#### Scenario: User right-clicks an unselected tree item
- **WHEN** the user right-clicks a valid tree item that is not currently selected
- **THEN** that tree item becomes the selected browser location before the context menu is displayed
- **THEN** the system displays the tree-item context menu at the invocation location

#### Scenario: User right-clicks empty tree space
- **WHEN** the user right-clicks within the tree control without hitting a valid tree item
- **THEN** the system does not display the tree-item context menu
- **THEN** the existing tree selection remains unchanged by the context-menu request

### Requirement: Context menu command surface
The system SHALL present the tree-item context menu as a native popup containing the following entries and separators in order: `Add New Disk Image`, `Add New Disk Group`, `Update All Disk Images`, separator, `Find in This Catalog`, separator, `Edit Description`, separator, `Add New Catalog`, `Open Catalog`, `Save Catalog`, `Rebuild Catalog File`, `Close Catalog`, separator, `Catalog Manager`, `Catalog Setup`, and `Properties`.

#### Scenario: User opens a tree-item context menu
- **WHEN** the context menu is displayed for a valid tree item
- **THEN** all specified command labels are visible in their specified order
- **THEN** separator boundaries appear after `Update All Disk Images`, `Find in This Catalog`, `Edit Description`, and `Close Catalog`

### Requirement: Shared implemented action routing
The system SHALL dispatch context-menu commands whose functionality is already implemented through their existing application command IDs and action handlers, without duplicating the action implementation or changing existing menu, toolbar, or accelerator routes.

#### Scenario: User chooses an already implemented context action
- **WHEN** the user selects `Add New Disk Image`, `Add New Catalog`, `Open Catalog`, or `Save Catalog` from the tree-item context menu
- **THEN** the application invokes the same existing action route used for the corresponding implemented application command
- **THEN** existing command guards and resulting workflow behavior remain applicable

#### Scenario: User uses an existing non-context route after the menu is added
- **WHEN** the user invokes an already implemented action from its existing main-menu, toolbar, or accelerator route
- **THEN** that route continues to invoke its established action behavior

### Requirement: Deferred context actions
The system SHALL display requested context-menu actions for which no implemented behavior exists as placeholders, SHALL use existing clearly identified inert command IDs where they already exist, SHALL add clearly named placeholder command IDs where no command ID exists, and SHALL NOT perform an operation for a placeholder selection.

#### Scenario: User selects an existing-surface placeholder
- **WHEN** the user selects `Find in This Catalog`, `Edit Description`, `Rebuild Catalog File`, `Close Catalog`, `Catalog Manager`, `Catalog Setup`, or `Properties` before its shared application behavior is implemented
- **THEN** the selected entry performs no catalog, disk, file, description, search, or settings operation

#### Scenario: User selects a context-only placeholder
- **WHEN** the user selects `Add New Disk Group` or `Update All Disk Images` before its behavior is implemented
- **THEN** the selected entry performs no catalog or disk operation
- **THEN** its reserved command identifier is recognizable in source as an intentionally unimplemented placeholder

### Requirement: Menu state and imagery reuse
The system SHALL preserve existing availability behavior for shared commands and SHALL reuse existing menu command imagery only where the application's native menu presentation already supports imagery for that command; placeholder commands SHALL NOT require new icons.

#### Scenario: Existing command availability is provided to the popup
- **WHEN** an existing application availability decision marks a reused command unavailable while the tree-item context menu is being displayed
- **THEN** the corresponding context-menu item is displayed disabled

#### Scenario: No placeholder icon exists
- **WHEN** a placeholder context-menu item has no existing icon applicable to native menu presentation
- **THEN** the item remains visible without requiring a new icon
