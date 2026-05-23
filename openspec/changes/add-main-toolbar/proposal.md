## Why

Where Is That? exposes its catalog and item actions through the main menu only, which makes frequent browsing commands slower to reach and leaves the window without the expected desktop toolbar affordance. A compact native toolbar gives users direct access to current commands while establishing visible placeholders for planned item and view operations.

## What Changes

- Add a native toolbar directly below the main menu bar, arranged into the requested command groups with separators.
- Present 25x25 toolbar buttons with centered, transparent 16x16 Fugue PNG icons and native tooltip text.
- Route toolbar actions that already exist to the same command identifiers and handlers as their menu counterparts, including catalog creation/opening and search.
- Add inert toolbar placeholders for commands that are not yet functional, so their presence does not change catalogs, files, preferences, or displayed results.
- Add a split/dropdown-style View Details button whose menu exposes View List and View Details commands, reusing menu command identifiers even while those commands remain placeholders.
- Add pressed/unpressed toggle affordances for the five sorting buttons, with no sorting effect until sorting functionality is implemented.
- Package only the selected Fugue icon assets required for the toolbar through the existing Visual Studio resource/build path.
- Add `Ctrl+R` for Report Generator through the same existing command identifier used by its main-menu entry, while preserving the existing `Ctrl+D` behavior for `Add/Update Disk Image`.

## Capabilities

### New Capabilities
- `main-toolbar`: Defines the main-window toolbar layout, sizing and icon presentation, shared command routing, dropdown view affordance, placeholder/toggle behavior, and icon packaging.

### Modified Capabilities

## Impact

- Affected implementation is expected in native resources and main-frame layout/command routing under `src/app`, with Windows Common Controls toolbar behavior remaining within the existing Win32 UI architecture.
- The Visual Studio resource/build definition will include the chosen 16x16 Fugue PNG files under the project's resource or asset convention; this adds packaged artwork but no new runtime library.
- Existing menu behavior, catalog/database behavior, search implementation, asynchronous scanning, and paged list browsing remain unchanged.
