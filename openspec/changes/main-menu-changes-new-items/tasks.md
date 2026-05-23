## 1. Native Menu Resources

- [x] 1.1 Add command and dialog resource identifiers for all new View, Search, Actions, and Options entries and for the General Settings dialog in `src/app/resource.h`.
- [x] 1.2 Extend the main `MENU` resource in `src/app/app.rc` so View, expanded Search, Actions, and Options appear after Edit in the specified order while Help remains available.
- [x] 1.3 Add the requested visible shortcut labels for `Search for Items`, `View File`, `Edit Description`, and `Properties` without binding placeholder functionality.

## 2. General Settings Persistence And Dialog

- [x] 2.1 Define the initial General Settings preference controls, defaults, and stable INI section/key names before writing dialog behavior, since the requested menu list does not enumerate those fields.
- [x] 2.2 Implement a focused native settings helper outside SQLite storage that consistently reads and writes the local `settings.ini` file with defaults for absent keys or an absent file.
- [x] 2.3 Define and implement the native General Settings dialog so it loads implemented preferences, persists confirmed changes through the settings helper, and discards cancelled edits.
- [x] 2.4 Route only `Options > General Settings` from the new commands to dialog behavior, leaving all other new menu entries without filesystem, catalog, display, or settings side effects.

## 3. Documentation And Verification

- [x] 3.1 Update `docs/spec/ui.md` and `docs/spec/storage.md` to record the expanded native menu surface and the separate `settings.ini` application-preference persistence contract.
- [x] 3.2 Build `whereisthat.sln` for `Release|x64` with MSBuild and confirm the resource, settings, and dialog changes compile without additional dependencies.
- [ ] 3.3 Smoke test menu ordering and labels; inert placeholder selection; opening, saving, reloading, and cancelling General Settings; absent-file defaults; and isolation of `settings.ini` from `catalog.db`.
