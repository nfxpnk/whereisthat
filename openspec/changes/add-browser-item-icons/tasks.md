## 1. Browser Artwork And Contract

- [x] 1.1 Update `docs/spec/ui.md`, storage/database documentation, and applicable acceptance documentation to require catalog/disk/content icons and reliable fixed-volume HDD/SSD classification.
- [x] 1.2 Copy `database-sql.png`, `drive.png`, `folder.png`, `document.png`, and `vise-drawer.png` from `Q:\fugue-icons-3.5.6\icons` into the existing Fugue application asset location, retaining the existing Fugue attribution/notice convention and original 16x16 transparent PNG artwork.
- [x] 1.3 Add native resource identifiers, `app.rc` entries, and Visual Studio project image entries required to package the five browser PNG resources in supported builds.

## 2. Native Icon List And Item Mapping

- [x] 2.1 Add browser image-list ownership and lifecycle to `MainWindowChrome`, reusing or factoring the WIC PNG loading path to populate a transparent 16x16 image list and attach it to the native TreeView and right-pane ListView.
- [x] 2.2 Expose stable browser image indices to UI presenters and update `CatalogTreeView` insertion so catalog roots use `database-sql.png`, disk/media nodes use `drive.png`, ordinary descendant folders use `folder.png`, and archive-backed folder nodes use `vise-drawer.png`.
- [x] 2.3 Add a focused case-insensitive retained-archive-file classifier aligned with the scanner archive-candidate extensions, using stored filename/extension metadata and no live source-media reads or schema changes.
- [x] 2.4 Update owner-data `FileListView`/display-info handling so root disk/media rows return the drive image and location content rows return the folder, document, or archive image alongside existing text while leaving database-backed paging unchanged.

## 3. Fixed Drive Classification

- [x] 3.1 Update native volume metadata collection to persist `HardDisk` or `SolidStateDisk` when Windows reliably reports the fixed volume seek-penalty property, retaining existing virtual/removable behavior and `Other` fallback.
- [x] 3.2 Extend verification coverage for fixed volume classification behavior where the native property can be injected or observed without changing the persisted disk-type allowed values.

## 4. Verification

- [x] 4.1 Build the supported `Release|x64` application configuration and confirm that the five PNG resources load with correct transparent rendering in both native browser controls.
- [ ] 4.2 Using `H:\archives.db`, manually verify catalog/database and disk/media entries display `database-sql.png` and `drive.png`, and a selected location containing ordinary folder/file and archive-backed rows displays `folder.png`, `document.png`, and `vise-drawer.png`, with archive-folder navigation unchanged.
- [ ] 4.3 Verify browser navigation remains functional from stored catalog data when source media is unavailable and that virtual-list paging and root disk inventory metadata display are not changed by icon presentation.
