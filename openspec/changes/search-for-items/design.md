## Context

`Search for Items` already appears in the native Search menu but is specified as a placeholder. Indexed item rows live in the active SQLite catalog database and the existing file browser uses an owner-data ListView with storage-backed paging. Search must remain useful for large catalogs and when original media is offline.

## Goals / Non-Goals

**Goals:**
- Open a native search window from the existing Search command and `Ctrl+F`.
- Match a user-entered substring against file and folder names across all media sources in the active database.
- Display results directly below the search options using paged retrieval.
- Preserve layer ownership: SQL in storage and Win32 presentation in UI/application code.

**Non-Goals:**
- Search by metadata other than item name, advanced wildcards, or result actions.
- Implement the other Search commands.
- Search across multiple catalog database files at once.

## Decisions

- Add a modal Win32 `SearchDialog` in `src/ui`, backed by a resource-defined dialog and owner-data ListView. This follows the existing native UI while keeping the search results visible below the criteria; replacing the main file pane would lose browsing context.
- Add count and page storage operations using prepared `LIKE` queries over `files.name`, with `%` and `_` escaped so entered text is treated as a literal substring. This keeps user values bound and lets the virtual result list avoid materializing every match; filtering a loaded UI list would not scale.
- Scope search to all `files` rows in the currently opened database. An active database is the application's catalog boundary, while limiting results to the selected media source would not satisfy searching all indexed items.
- Show the same useful item metadata columns as the main file list. Paths distinguish same-named results without introducing a new joined display model.

## Risks / Trade-offs

- Name-substring searches beginning with a wildcard pattern cannot fully use the existing name index -> retain paging and keep the first implementation simple; later search-index work can refine performance.
- A modal search window does not allow simultaneous main-frame navigation -> it provides a focused lookup interaction and can later become modeless if required.
- SQLite's default case-insensitive comparison has limited non-ASCII folding -> use native stored text behavior now and avoid promising language-aware matching.

## Migration Plan

No database migration is required. Existing catalog databases already contain the queried `files.name` field and indexes; the feature consists of new read queries and UI wiring.

## Open Questions

None block this implementation.
