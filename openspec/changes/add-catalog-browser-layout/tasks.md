## 1. Browser Data Access

- [x] 1.1 Add storage-layer count and paged retrieval operations for immediate contents of the virtual catalog root and a stored source/folder location, preserving owner-data `ListView` paging.
- [x] 1.2 Add storage-layer folder-child retrieval for lazy tree expansion using existing source IDs, stored parent paths, and directory metadata without schema changes.

## 2. Native Browser Views

- [x] 2.1 Replace or adapt the left-pane source presentation as a native folder `TreeView` that displays the active catalog root, indexed source folders, and lazily expanded descendant folders only.
- [x] 2.2 Adapt the right owner-data `ListView` to bind to a browser location and display only its immediate stored folders and files, including folder activation output needed for navigation.
- [x] 2.3 Add native Back, Forward, and read-only address controls above the right contents list and incorporate them into the current splitter/layout resizing behavior.

## 3. Navigation Orchestration

- [x] 3.1 Add main-frame browser location and session-history state, including catalog-relative address formatting and enabled-state updates for Back and Forward.
- [x] 3.2 Wire tree selection/expansion, contents folder activation, and Back/Forward commands so tree selection, right contents, and address display remain synchronized.
- [x] 3.3 Reset or refresh browser tree, location, and history correctly on catalog activation, empty catalog state, and scan completion without relying on connected source media.

## 4. Documentation And Verification

- [x] 4.1 Update canonical UI and storage documentation to describe hierarchical catalog browsing, native navigation controls, offline display behavior, and path-scoped paged queries.
- [x] 4.2 Build the Release x64 application.
- [ ] 4.3 Manually verify catalog-root browsing, folder tree expansion, immediate contents display, folder activation, and Back/Forward/address synchronization for indexed data.
