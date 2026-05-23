## Why

Where Is That? currently has no dedicated main application icon, so the executable, window chrome, taskbar entry, and Alt-Tab preview fall back to generic Windows visuals. A recognizable icon makes the native app feel finished and easier to identify after installation or while multiple windows are open.

## What Changes

- Add a main application icon asset suitable for Windows executable resources.
- Register the icon in the Win32 resource script and resource header.
- Ensure the main window uses the application icon for large and small icon surfaces.
- Include the icon resource in the MSVC project/build inputs where required.
- Preserve existing menu, scanning, storage, and UI behavior.

## Capabilities

### New Capabilities
- `main-app-icon`: Defines how the application exposes and applies its primary icon across Windows shell and window surfaces.

### Modified Capabilities

## Impact

- Affected files are expected around `src/app/resource.h`, `src/app/app.rc`, `src/app/MainFrame.*`, and the MSVC project file if the icon asset must be explicitly listed.
- Adds a Windows `.ico` asset to the repository.
- No API, database, or runtime dependency changes are expected.
