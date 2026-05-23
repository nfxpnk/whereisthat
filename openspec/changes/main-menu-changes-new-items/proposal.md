## Why

After the File and Edit menus are established, the main window still needs the rest of the requested application command surface. Adding View, Search, Actions, and Options now provides the intended native menu structure, while implementing General Settings gives the first Options command real value through durable preferences.

## What Changes

- Add `View`, `Search`, `Actions`, and `Options` top-level menus after `Edit`, containing the requested command items and visible keyboard shortcut text where specified.
- Add `Sort items` under View as a placeholder submenu/entry point without defining sort options or behavior in this change.
- Treat all newly introduced menu commands as placeholders with no catalog, file, or settings effect except `Options > General Settings`.
- Implement a native `General Settings` workflow opened from the Options menu.
- Store application settings managed by General Settings in a local `settings.ini` file and restore persisted values when the application is launched again.

## Capabilities

### New Capabilities
- `additional-main-menu-commands`: Defines the View, Search, Actions, and Options menu layout, labels, displayed shortcuts, and placeholder behavior following the existing File/Edit command surface.
- `general-settings`: Defines opening General Settings and persisting application preference values through `settings.ini`.

### Modified Capabilities

## Impact

- Depends on the File/Edit menu foundation described by `main-menu-changes`, because the new top-level menus are placed after `Edit`.
- Affected implementation is expected in Win32 resources and main-frame command routing under `src/app`, with the native settings dialog under the established app/UI boundary.
- Adds a non-database application preference file, `settings.ini`; `catalog.db`, SQLite deployment, asynchronous scanning, and paged catalog browsing remain unchanged.
