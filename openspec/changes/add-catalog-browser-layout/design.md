## Context

The main frame already divides its client area into a left catalog-source `ListView` and a right owner-data file `ListView`. For one selected source, the right list currently retrieves every indexed row rather than only immediate children. Each active SQLite database is the product catalog, while the compatible `catalogs` table contains its indexed source roots; scanning stores a directory row for each source with an empty `parent_path`, then stores descendants with their physical parent paths.

The new browser must remain pure Win32/Common Controls, present persisted metadata even when source media is unavailable, and continue paging right-pane results for large catalogs. SQL remains in `src/storage`, native view adapters in `src/ui`, and navigation orchestration in `src/app`.

## Goals / Non-Goals

**Goals:**
- Present the active catalog as a minimal Explorer-style tree and contents view.
- Browse the virtual catalog root, indexed source roots, and descendant stored folders without filesystem access.
- Keep tree selection, right-pane activation, address display, and Back/Forward history consistent.
- Retrieve only location children and load right-pane rows through database-backed paging.
- Fit the feature into the current main-frame, storage, and model boundaries.

**Non-Goals:**
- Copy, paste, rename, delete, drag and drop, context menus, thumbnails, preview, sorting controls, or shell launching.
- Editable address navigation or filesystem probing of catalogued source paths.
- Restructuring or migrating the `catalogs` and `files` tables.
- Combining browsing history with search dialog state or persisting history across sessions.

## Decisions

- Treat the active SQLite file as a virtual catalog-root location displayed using its file name, without adding stored catalog-display metadata. Its immediate children are the indexed source root directory rows already recorded with `parent_path=''`; below a source root, folders are addressed by source ID plus stored path. Alternative considered: use each `catalogs` row as a top-level root as the current left list does, rejected because it does not provide the requested single catalog parent node.
- Replace the left source `ListView` with a native `TreeView` adapter that initially inserts the catalog root and source-folder children, and populates descendant folder nodes on expansion using path-scoped storage queries. Tree nodes retain location identity needed to load or select that location, and files are never inserted into this tree. Alternative considered: preload all directories, rejected because large catalogs should not require full hierarchy materialization.
- Retain an owner-data native `ListView` for the right contents pane, but bind it to a browser location rather than an entire source. At the virtual root it lists source root folders; at a source/folder location it pages the immediate stored child files and folders using `catalog_id` and `parent_path`. Alternative considered: filter existing all-source list rows in the UI, rejected because it breaks paging and loads irrelevant rows.
- Add a lightweight navigation header above the right list using native Back and Forward buttons plus a read-only text/edit control for the address. The address renders a catalog-relative breadcrumb-style path such as `MyCatalog\Source\Subfolder`, never a live filesystem navigation target. Alternative considered: an editable address box, rejected because resolving arbitrary typed catalog paths is outside minimal browsing.
- Introduce a main-frame browser location/history state: the virtual root or a stored source/folder identity, a visited-location vector, and current index. Navigating by tree selection or right-pane folder activation appends a location after discarding forward history; Back and Forward restore entries without creating additional history and update both panes and enabled buttons. Alternative considered: use only tree selection as state, rejected because it cannot express browser history cleanly.
- Use ordinary native contents-list folder activation, such as double-click or keyboard activation, to navigate into a folder; a single selection alone does not navigate. This matches basic Explorer interaction and allows item selection without unexpectedly changing location.
- On catalog activation, scan completion refresh, or empty state, rebuild/reset the browser binding and history from the active database state. Catalog root selection shows current indexed source roots; an empty or newly created catalog shows an empty contents list while retaining its root when active. Alternative considered: retain stale history across catalog switches, rejected because its locations refer to different data.
- Add prepared count/page and child-folder read operations in `src/storage` and adapt UI/model ownership only as needed for source/location identity. No database schema change is required because `catalog_id`, `parent_path`, `is_directory`, and the existing root directory rows encode the required hierarchy.

## Risks / Trade-offs

- A catalog can contain source roots with identical display names -> retain source IDs and stored roots as location identity even if visible labels match.
- Physical source paths stored in older scans may vary in separator/casing representation -> navigate using the exact stored path relationships associated with the source rather than trying to read or normalize live media.
- Lazy tree expansion and a concurrently completed scan could leave shown nodes stale -> reset or refresh the browser tree and current listing when scan completion reloads catalog content.
- Displaying the active database file name as its catalog label provides no editable friendly name -> keep this minimal behavior until rename/catalog metadata is specified separately.

## Migration Plan

1. Add path-scoped read operations for root-source folders, immediate contents, and directory-child discovery using existing indexed rows.
2. Replace the main-frame source list binding with tree, contents-navigation, and history state while retaining owner-data file rendering.
3. Create the navigation controls and wire tree expansion/selection and contents activation notifications.
4. Refresh canonical UI and storage documentation and validate native browsing behavior in a Release x64 build.

No SQLite migration is required. Rolling back the UI leaves catalog databases readable by the prior implementation because no stored data representation changes.

## Open Questions

None block implementation. A future catalog rename feature can replace the file-name-derived root display label without altering browsing identity.
