## Why

The Search menu currently exposes `Search for Items` as an inactive placeholder, so users cannot locate indexed files or folders without manually browsing each media source. A native search window makes the catalog useful for its primary lookup workflow.

## What Changes

- Make `Search > Search for Items` and its displayed `Ctrl+F` shortcut open a native search window.
- Provide a name search input and Search action in that window.
- Display matches for file and folder names immediately below the search options, across all indexed media sources in the active catalog database.
- Keep large result sets usable with database-backed, owner-data result paging.
- Leave the remaining Search menu entries as placeholders.

## Capabilities

### New Capabilities
- `item-search`: Defines the native Search for Items window and active-catalog filename/folder-name query results.

### Modified Capabilities

## Impact

- Native resources and command routing in `src/app`.
- A search-result ListView adapter in `src/ui`.
- New paged prepared-statement queries in `src/storage`.
- Canonical UI, storage, and feature documentation.
