## Context

The implemented main menu currently provides File, Edit, a one-item Search menu, and Help in `src/app/app.rc`; command routing is centralized in `MainFrame::OnCommand()`. The earlier `main-menu-changes` change established File/Edit, functional catalog creation, and disk-image scanning. This follow-on change needs to insert the remaining requested native menus after Edit, replace the provisional Search content, and make only `Options > General Settings` functional. The application remains pure Unicode Win32/C++20 with SQLite reserved for catalog data.

The request establishes `settings.ini` as the persistence file but does not enumerate the initial preference fields in the General Settings dialog. This implementation begins with one existing UI concern, status-bar visibility, exposed as `Show status bar`, enabled by default, and persisted as `General.ShowStatusBar`.

## Goals / Non-Goals

**Goals:**
- Present the requested View, Search, Actions, and Options menus in native Win32 resources with their exact ordering and visible shortcut labels.
- Preserve the implemented File/Edit workflows and existing Help behavior.
- Open a native General Settings dialog and persist its confirmed preference values in `settings.ini`.
- Ensure all other newly introduced menu entries are inert placeholders.

**Non-Goals:**
- Implement view modes, searching, comparison, file/catalog actions, toolbar or status-bar toggling, or the four non-general Options commands.
- Store application preferences in SQLite or change the catalog database schema.
- Add behavior for preferences other than the initial `Show status bar` control.
- Add a managed UI layer, a third-party settings library, or a new runtime dependency.

## Decisions

- Extend the existing `MENU` resource with View, expanded Search, Actions, and Options popups between Edit and Help, and allocate distinct command IDs for every entry. Text for requested shortcut presentation uses native tab-separated labels (`Ctrl+F`, `Ctrl+I`, `Ctrl+E`, and `Ctrl+P`). Alternative considered: construct the menus dynamically, rejected because the current resource-defined menu is clearer and already supplies native layout.
- Treat `Sort items` as a single inert menu command in this change because no child sort modes were requested. Alternative considered: introduce guessed sort choices, rejected because that would create unrequested user-visible behavior.
- Do not add accelerator bindings for shortcut labels on placeholder commands. The label is displayed as requested, but no keyboard invocation is treated as implemented until its behavior exists. Alternative considered: route accelerators to no-op command handlers, rejected because that would imply functional shortcuts without a workflow.
- Route only `ID_OPTIONS_GENERAL_SETTINGS` to a new modal Unicode Win32 dialog. All other newly allocated IDs either have no route or explicitly return without changing state. Alternative considered: display "not implemented" notices for placeholders, rejected because placeholder selection is specified to have no effects.
- Add a small INI settings boundary outside `src/storage`, using Unicode Win32 INI APIs or an equivalently focused native helper to read and write `settings.ini`. SQLite remains responsible only for catalog data. The path follows the application's existing local-file convention and must resolve to the named local settings file consistently for reading and writing. Alternative considered: add preferences to `catalog.db`, rejected because the requested persistence file is explicitly `settings.ini`.
- Have General Settings load default values when keys or the file are absent, initialize controls from loaded values, and write only when the dialog is confirmed. Cancellation discards in-dialog edits. Alternative considered: write on every control change, rejected because it makes Cancel unable to preserve stored settings.
- Expose `Show status bar` as the initial implemented preference, backed by `General.ShowStatusBar` with a default of enabled. It controls the existing status display and can be verified immediately without introducing catalog or file-management semantics. Alternative considered: define a speculative catalog-operation setting, rejected because no such workflow has been requested.
- Keep the store keyed and extensible: the dialog owns which general preference keys it exposes, while the settings helper serializes only those implemented values under stable INI sections/keys. Alternative considered: define keys for placeholder menus up front, rejected because no settings behavior has been requested for them.

## Risks / Trade-offs

- Displayed shortcuts for placeholder Search/Actions commands are not actionable yet -> verify their labels while documenting that only General Settings gains behavior in this change.
- An inconsistent working directory could place a relative `settings.ini` unexpectedly -> compute one stable local path once and use it for all settings reads and writes.
- Adding many resource identifiers could accidentally route a placeholder into existing behavior -> use unique IDs and smoke test that placeholders leave catalog and settings state unchanged.

## Migration Plan

No catalog database migration is required. Add menu and dialog resources, add the settings persistence boundary and General Settings routing, and update UI/storage documentation for the new preference file. Existing installs without `settings.ini` start with dialog defaults; the file is created only when confirmed settings are written. Reverting the UI code leaves an existing `settings.ini` as harmless local preference data.

## Open Questions

- No open technical decision blocks implementation of this change. Additional preferences can be proposed when their behavior is specified.
