## Context

The main browser is built from native controls created by `MainWindowChrome`: a folder-only `WC_TREEVIEWW` and an owner-data `WC_LISTVIEWW` whose text is supplied through `FileListView`. `CatalogTreeView` inserts stored folder nodes and already receives `FileEntry::isArchive` for expanded archive-backed folders; `FileListView` receives both folder rows and ordinary file rows. Existing archive scanning distinguishes readable archive-backed folders through persisted `entry_type='archive'`, while retained physical archive files retain their filename and extension. The application already embeds and WIC-decodes 16x16 Fugue PNG resources for the toolbar.

This primarily adds native presentation and also corrects an exposed metadata gap: `PopulateVolumeMetadata` currently assigns only removable types, leaving fixed scans as the default `Other`. It must preserve database-backed paging, offline browsing, archive activation behavior, and native image transparency. The supplied icon source directory contains `database-sql.png`, `drive.png`, `folder.png`, `document.png`, and `vise-drawer.png`.

## Goals / Non-Goals

**Goals:**

- Display a 16x16 type icon before catalog roots, disk/media entries, and stored folder/file names in the main tree and right-pane list.
- Make both retained archive files and expanded archive-backed folders visually distinct with `vise-drawer.png`.
- Reuse native PNG resource/image-list handling and existing persisted/display type data.
- Package only the required Fugue PNG assets with attribution already used for other Fugue assets.
- Persist reliable `HardDisk` or `SolidStateDisk` values when a selected fixed scan volume exposes seek-penalty metadata.

**Non-Goals:**

- Add icons to the search-results dialog, menus, or toolbar actions.
- Change archive scanning, folder/file representation, schema, navigation, sorting, or paging.
- Resolve archive identity by reading source media during browsing.

## Decisions

- Vendor `database-sql.png`, `drive.png`, `folder.png`, `document.png`, and `vise-drawer.png` from `Q:\fugue-icons-3.5.6\icons` beside the existing Fugue resources under `src/app/toolbar/fugue/`, preserving the source PNGs and existing notice convention. Embed them as RCDATA resources and list them in `WhereIsThat.vcxproj`, as done for toolbar PNGs. Alternative considered: load the external `Q:` path at runtime; rejected because released builds must be self-contained and the source directory is a development input only.

- Give `MainWindowChrome` ownership of a separate 16x16 browser image list and attach it to the TreeView with `TVSIL_NORMAL` and the content ListView with `LVSIL_SMALL`. Factor or reuse the existing WIC resource PNG decode path so both toolbar and browser assets retain alpha; destroy the browser list with the controls. Alternative considered: create image lists independently in each UI presenter; rejected because the controls are created and destroyed together by the chrome owner and need identical image-index ordering.

- Define stable presentation indices for ordinary folder, ordinary document, archive container, catalog database, and disk/media images and make those indices available to `CatalogTreeView` and `FileListView` without giving either class resource ownership. `CatalogTreeView::InsertNode` supplies `TVIF_IMAGE | TVIF_SELECTEDIMAGE`: catalog roots use the database image, disk/media roots use the drive image, and descendant folders choose folder or archive from existing `isArchive` data. Alternative considered: represent catalog and disk/media entries as folders; rejected because their semantics and requested artwork are distinct.

- Extend owner-data ListView display handling to return `LVIF_IMAGE` in addition to column text. Catalog-root disk inventory rows use the drive image; directory rows use folder or archive image according to `isArchive`; non-directory rows use archive imagery when their persisted filename/extension matches the archive-container classification, otherwise document imagery. Alternative considered: materialize list items solely to set images; rejected because it would break the existing virtual paging design.

- Use a focused shared case-insensitive archive-name classifier for retained file presentation, aligned with the scanner's supported candidate extension set (`zip`, `7z`, `rar`, `tar`, `tgz`, `gz`, `bz2`, `xz`, `cab`, `arj`, `lha`, and `iso`). This permits an archive left as a normal file row to use `vise-drawer.png` even when it was not expanded or was unreadable, using only persisted display metadata. Alternative considered: add an `is_archive_file` database column; rejected because icon classification does not need a catalog format change.

- For a non-virtual volume reported by `GetDriveTypeW` as `DRIVE_FIXED`, query `StorageDeviceSeekPenaltyProperty` through the native volume handle. Persist `SolidStateDisk` when the property explicitly reports no seek penalty and `HardDisk` when it reports a seek penalty; retain `Other` if that native property cannot be obtained. Preserve `VirtualDisk` for selected ISO media and `RemovableUSB` for removable sources. Alternative considered: map every `DRIVE_FIXED` value to `HardDisk`; rejected because it would incorrectly label SSD volumes when the property query is unavailable.

- Add the icon and fixed-media-type rules to maintained UI/storage/acceptance documentation and verify icon presentation using `H:\archives.db`, whose scanned content provides ordinary and archive-backed browser rows. Alternative considered: document only the code implementation; rejected because icon and type identity are user-visible behavior.

## Risks / Trade-offs

- [The retained-file archive icon relies on file extension rather than validated contents] -> Use the same established archive-candidate extension set used for scanner probing and describe the icon as archive-container presentation rather than a guarantee of readable content.
- [PNG alpha or image-list lifetime errors could yield missing or black-backed icons] -> Reuse the current WIC/32-bit image-list pattern, attach owned lists once, and manually inspect native rendering before completion.
- [Adding images to an owner-data list could accidentally interfere with pagination or column rendering] -> Supply only image indices in `LVN_GETDISPINFO` alongside the existing paged entry lookup and test large/normal list navigation unchanged.
- [Archive-backed folders depend on the active archive-scanning work being available] -> Base icon selection on existing projected `isArchive`; ordinary folder/file icons remain functional independently and archive verification runs once archive rows exist.
- [Some fixed devices or drivers may not expose seek-penalty metadata] -> Retain `Other` when the Windows storage query fails rather than persisting an invented HDD/SSD subtype.

## Migration Plan

1. Copy and embed the five selected Fugue PNG resources and expose native browser image indices.
2. Create and attach the browser image list in `MainWindowChrome`, then add tree and owner-data list item-image selection.
3. Add the shared retained-archive-file classification and fixed-volume media classification; update maintained UI/storage/acceptance documentation.
4. Build the supported x64 configuration and manually verify catalog, disk/media, ordinary-folder/file, and archive imagery using `H:\archives.db`.

No database or user-settings migration is required. Rolling back removes the browser icon resources/image-list plumbing and leaves stored catalogs unchanged.

## Open Questions

None block implementation.
