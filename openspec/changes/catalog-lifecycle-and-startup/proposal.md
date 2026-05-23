## Why

The application currently treats `catalog.db` as a container for multiple in-database catalog rows, so `File > New Catalog` does not create an independent catalog file and scanning can continue modifying the same database. Users need catalogs to be separate SQLite database files that they can create, open, and resume explicitly.

## What Changes

- **BREAKING** Replace the single fixed `catalog.db` persistence assumption with an active-catalog model in which each selected SQLite database file is one separate catalog.
- Make `File > New Catalog` prompt for a destination, create a new empty SQLite catalog database at that path, switch the active catalog to it, and leave any previously active database unchanged.
- Make `File > Open` select and activate an existing SQLite catalog database.
- Store the last successfully opened or created catalog path in application settings and attempt to reopen it on application startup.
- Display the last-opened catalog in General Settings and persist up to ten recently activated catalogs for access through `File > Open Recent`.
- Start with no active catalog when no prior path is stored or the saved path cannot be opened, while keeping New/Open available to enter an active-catalog state.
- Define `Edit > Add/Update Disk Image` as an operation against only the active catalog, updating existing contents for the same selected media source rather than creating duplicates.
- Revise product, storage, architecture, UI, and acceptance documentation that currently names `catalog.db` as the permanent application catalog or describes New Catalog as inserting an in-database row.

## Capabilities

### New Capabilities
- `catalog-database-lifecycle`: Defines separate SQLite catalog files, active-catalog switching through New/Open, empty-state command behavior, and active-catalog-only Add/Update Disk Image updates.
- `last-used-catalog-startup`: Defines persistence of the last-used catalog path in application settings and conditional restoration of that active catalog at startup.

### Modified Capabilities

## Impact

- Revises the earlier File/Edit menu workflow that creates catalog rows in the fixed `catalog.db` file.
- Affects main-frame startup and command routing in `src/app`, settings persistence in `src/platform`, SQLite connection/catalog data operations in `src/storage`, and scanning orchestration in `src/core`.
- Requires documentation updates in `docs/spec/` to replace the fixed-database model while retaining native Win32, C++20, SQLite C API, asynchronous scan, and paged browsing constraints.
- Existing SQLite files, including a file named `catalog.db`, remain potential catalog files; implementation must avoid silently treating that name as a mandatory default catalog.
