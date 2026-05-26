## ADDED Requirements

### Requirement: Folder and file type icons in the main browser
The system SHALL display a 16x16 Fugue icon before each stored folder or file name shown in the main browser folder tree or location contents list, using `folder.png` for ordinary folders, `document.png` for non-archive files, and `vise-drawer.png` for archive containers.

#### Scenario: Ordinary folder is displayed
- **WHEN** an ordinary stored folder is shown as a tree node or as a row in a selected location's contents list
- **THEN** its displayed name is preceded by the 16x16 `folder.png` icon

#### Scenario: Ordinary file is displayed
- **WHEN** a stored non-archive file is shown in a selected location's contents list
- **THEN** its displayed name is preceded by the 16x16 `document.png` icon

#### Scenario: Archive-backed folder is displayed
- **WHEN** a readable archive represented as an archive-backed stored folder is shown in the tree or a location contents list
- **THEN** its displayed name is preceded by the 16x16 `vise-drawer.png` icon
- **THEN** the item remains activatable and browseable as a folder

#### Scenario: Archive remains a file row
- **WHEN** an archive-container file recognized from stored display metadata is shown in a selected location's contents list rather than as an expanded archive-backed folder
- **THEN** its displayed name is preceded by the 16x16 `vise-drawer.png` icon
- **THEN** the item remains a file row and does not become browseable merely because of its icon

### Requirement: Catalog and disk media icons in the main browser
The system SHALL display `database-sql.png` before each catalog-database root name and SHALL display `drive.png` before each indexed disk/media name shown in the tree or catalog-root disk inventory.

#### Scenario: Catalog database root is displayed
- **WHEN** an opened catalog database is shown as a top-level tree root
- **THEN** its displayed name is preceded by the 16x16 `database-sql.png` icon

#### Scenario: Disk media entry is displayed in the tree
- **WHEN** an indexed disk/media source is shown below its catalog database root in the tree
- **THEN** its displayed name is preceded by the 16x16 `drive.png` icon

#### Scenario: Disk media inventory row is displayed
- **WHEN** the catalog root is selected and its indexed disk/media rows are shown in the right-pane inventory
- **THEN** each disk/media name is preceded by the 16x16 `drive.png` icon

### Requirement: Icon presentation preserves browser scope and data behavior
The system SHALL determine folder/file browser icons from persisted catalog presentation data and SHALL preserve existing offline navigation, native control behavior, database-backed paging, and archive representation semantics.

#### Scenario: Indexed media is unavailable
- **WHEN** an ordinary folder, ordinary file, or archive item is browsed from a stored catalog while its source media is disconnected
- **THEN** the applicable icon remains displayable with the stored item name
- **THEN** icon presentation does not require access to the original filesystem or archive bytes

#### Scenario: Paged contents list requests visible rows
- **WHEN** the owner-data contents list retrieves a page containing folders or files for display
- **THEN** each visible item receives its applicable type icon without materializing all items in the current location

#### Scenario: Catalog-root disk inventory remains paged
- **WHEN** the main browser displays the catalog-root disk inventory with drive icons
- **THEN** its existing database-backed paging and metadata columns remain unchanged

### Requirement: Packaged Fugue browser artwork
The system SHALL package the original 16x16 Fugue PNG files `database-sql.png`, `drive.png`, `folder.png`, `document.png`, and `vise-drawer.png` as application assets/resources and SHALL display them through native browser controls with transparency preserved.

#### Scenario: Browser icon assets are added
- **WHEN** implementation obtains the browser icon artwork from `Q:\fugue-icons-3.5.6\icons`
- **THEN** only the required source PNG files are copied into the application's native asset/resource structure with applicable Fugue attribution retained
- **THEN** the artwork is not upscaled or converted into an opaque replacement

#### Scenario: Application displays browser rows
- **WHEN** a built application shows tree or contents-list folder/file icons
- **THEN** the required icon resources are available without an external Fugue source-directory dependency
- **THEN** their transparent pixels render correctly in the native TreeView and ListView
