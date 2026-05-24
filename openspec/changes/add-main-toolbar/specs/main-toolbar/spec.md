## ADDED Requirements

### Requirement: Main toolbar placement and visual presentation
The system SHALL display a native toolbar directly below the main menu bar in the main window, with each toolbar button occupying a 25x25 pixel area and each action image rendered as a centered, transparent 16x16 PNG icon.

#### Scenario: Main window is displayed
- **WHEN** the user opens the main application window
- **THEN** the toolbar is visible immediately beneath the menu bar and above the catalog and file panes
- **THEN** its action buttons use 25x25 button areas with centered 16x16 transparent icons

#### Scenario: Main window is resized
- **WHEN** the user resizes the main window while the toolbar is present
- **THEN** the browsing panes remain laid out below the toolbar without overlapping it
- **THEN** the existing status-bar layout behavior remains available at the bottom of the window

### Requirement: Toolbar command order and grouping
The system SHALL present toolbar actions in the following order: Create New, Open, Save, Generate Report, separator, Search, separator, Edit Description, Item Properties, Open In Explorer, View File, separator, View Details, separator, Sort By Name, Sort By Extension, Sort By Size, Sort By Date, and Reverse Sort Order.

#### Scenario: User inspects the toolbar
- **WHEN** the main toolbar is visible
- **THEN** all toolbar action buttons appear in the required order
- **THEN** visual dividers separate the four specified button groups

### Requirement: Shared command dispatch
Toolbar controls that correspond to existing menu commands SHALL dispatch the same command identifier used by the matching menu item, and SHALL therefore have the same implemented or placeholder behavior as that menu item.

#### Scenario: User selects implemented catalog and search commands
- **WHEN** the user activates Create New, Open, or Search from the toolbar
- **THEN** the application invokes the same catalog creation, catalog opening, or item search workflow used by the corresponding main-menu command

#### Scenario: User selects an existing menu placeholder from the toolbar
- **WHEN** the user activates Save, Generate Report, Edit Description, Item Properties, Open In Explorer, or View File while its matching menu command remains unimplemented
- **THEN** the application dispatches the matching menu command identifier without performing new catalog, filesystem, settings, description, report, or display behavior

#### Scenario: A shared menu command gains functionality
- **WHEN** a menu command used by a toolbar button is implemented in a later change
- **THEN** activating that toolbar button reaches the same command handler without requiring duplicated business logic

### Requirement: Toolbar tooltips and shortcuts
The system SHALL expose action tooltips for toolbar buttons, including `Catalog` for Create New and Open and `Save Current Catalog` for Save, and SHALL route requested shortcuts through the same command identifiers as corresponding toolbar and menu actions while retaining established existing accelerator behavior.

#### Scenario: User hovers catalog buttons
- **WHEN** the user hovers Create New or Open
- **THEN** the toolbar tooltip displays `Catalog`

#### Scenario: User hovers Save
- **WHEN** the user hovers Save
- **THEN** the toolbar tooltip displays `Save Current Catalog`

#### Scenario: User uses existing functional shortcuts
- **WHEN** the user invokes `Ctrl+N`, `Ctrl+O`, or `Ctrl+F`
- **THEN** the existing New Catalog, Open, or Search for Items command behavior is retained

#### Scenario: User invokes the additional search alias
- **WHEN** the user presses `F3`
- **THEN** the application invokes the same Search for Items command used by the Search toolbar button and menu item

#### Scenario: User invokes shortcuts for current placeholder commands
- **WHEN** the user presses `Ctrl+S`, `Ctrl+R`, `Ctrl+E`, `Ctrl+P`, `Alt+Enter`, `F4`, or `Ctrl+I` while its corresponding Save, Report Generator, or Actions menu command remains unimplemented
- **THEN** the application dispatches the corresponding shared placeholder command identifier
- **THEN** the application performs no new catalog, filesystem, settings, description, report, or display behavior

#### Scenario: User invokes the existing disk-image shortcut
- **WHEN** the user presses `Ctrl+D`
- **THEN** the application retains the existing Add/Update Disk Image command behavior

### Requirement: View Details dropdown control
The system SHALL provide View Details as a split/dropdown-style toolbar control with a visible arrow region whose dropdown menu contains View List followed by View Details.

#### Scenario: User opens the view dropdown
- **WHEN** the user clicks the arrow region of the View Details toolbar control
- **THEN** a native dropdown menu displays `View List` and `View Details` in that order

#### Scenario: User chooses a view entry before view modes are implemented
- **WHEN** the user selects `View List` or `View Details` from the toolbar dropdown while the equivalent menu item remains a placeholder
- **THEN** the application dispatches the same placeholder command identifier
- **THEN** the file presentation remains unchanged

### Requirement: Sorting toggle placeholders
The system SHALL display Sort By Name, Sort By Extension, Sort By Size, Sort By Date, and Reverse Sort Order as toolbar buttons capable of pressed and unpressed visual states, and SHALL synchronize their pressed state to an authoritative sorting state when one exists.

#### Scenario: No item sorting behavior exists
- **WHEN** the toolbar is displayed before sorting functionality is implemented
- **THEN** all sort buttons are displayed in their unpressed state
- **THEN** selecting a sort placeholder toggles its pressed or unpressed visual state without reordering items or persisting sort state

#### Scenario: Sort state is implemented later
- **WHEN** an authoritative active sort mode or reverse-order state is supplied by the file-list behavior
- **THEN** the corresponding toolbar toggle state reflects that application state

### Requirement: Packaged Fugue PNG assets
The system SHALL use only the required transparent 16x16 PNG icons copied from the Fugue Icons repository for toolbar imagery, SHALL keep each copied source image at its original size, and SHALL include those images in the supported build/package output.

#### Scenario: Toolbar assets are added to the project
- **WHEN** an icon is selected for a toolbar action
- **THEN** it is a semantically appropriate existing 16x16 Fugue PNG stored in the project's native resources/assets structure
- **THEN** it is not upscaled or converted into an opaque replacement

#### Scenario: Application is built and run
- **WHEN** the supported Visual Studio/MSBuild build produces the application executable/package
- **THEN** every toolbar icon needed at runtime is available through the build's resource or packaged-asset path
- **THEN** each icon displays with preserved transparency in the toolbar
