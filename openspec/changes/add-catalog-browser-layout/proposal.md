## Why

The current main-window browser selects a scanned source from a flat list and shows all of its indexed rows, which does not let users browse the catalog as a folder hierarchy. A minimal Explorer-style surface makes offline catalog contents understandable and navigable without adding file-management features.

## What Changes

- Replace the main browsing surface with a two-column layout: a native folder `TreeView` on the left and a native contents `ListView` on the right.
- Represent the active catalog database as the top-level tree node, with indexed source roots and descendant folders available for expansion beneath it.
- Add a compact right-pane navigation bar with Back, Forward, and a read-only catalog-relative address display.
- Show files and folders immediately contained by the selected catalog location, and allow folder activation in the contents list to navigate deeper.
- Track browsing history so Back and Forward restore previously visited catalog locations, synchronized across the tree selection, contents list, and address display.
- Preserve offline browsing and large-list support with storage-backed reads of indexed metadata only; do not add editing, sorting, search, preview, or shell file operations.

## Capabilities

### New Capabilities
- `catalog-content-browser`: Defines hierarchical catalog folder browsing, the native two-pane presentation, and minimal location navigation/history behavior.

### Modified Capabilities

## Impact

- Main-frame control creation, resizing, notifications, and navigation orchestration in `src/app`.
- New or adapted native tree/content/navigation presentation in `src/ui`.
- Path-scoped and child-folder prepared queries in `src/storage`, using existing persisted catalog metadata without schema migration.
- Canonical UI/storage documentation and focused Release x64 verification.
