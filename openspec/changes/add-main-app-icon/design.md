## Context

Where Is That? is a native Win32/MSVC desktop app. Its resource script currently defines the main menu only, and `MainFrame::Create()` registers the window class with `LoadIcon(nullptr, IDI_APPLICATION)`, which displays the generic Windows application icon. The change should stay within the existing Win32 resource model and avoid introducing new runtime dependencies.

## Goals / Non-Goals

**Goals:**
- Add a repository-owned `.ico` asset for the main app icon.
- Register the icon as a Win32 resource with a stable resource identifier.
- Use the app icon for the executable shell icon and the main window large/small icon surfaces.
- Keep the existing MSVC build flow and resource script structure simple.

**Non-Goals:**
- Redesign branding beyond the main app icon asset.
- Add an installer, file associations, or per-document icons.
- Change scanning, catalog storage, menus, or list view behavior.

## Decisions

- Use a Windows `.ico` file rather than PNG-only assets. Windows executable resources and shell surfaces consume `.ico` directly, and a multi-size icon can cover taskbar, Alt-Tab, title bar, and Explorer surfaces without extra conversion at runtime. Alternative considered: embed PNG files and create icons dynamically, but that adds unnecessary code and resource handling.
- Add a dedicated icon resource ID such as `IDI_APPICON` in `resource.h` and reference it from `app.rc`. This keeps menu and icon resources explicit and avoids overloading existing menu IDs. Alternative considered: use `IDI_APPLICATION`, but that remains a system resource rather than an app-owned asset.
- Load the icon from the module instance during `RegisterClassExW` and set both `hIcon` and `hIconSm`. For the small icon, use `LoadImageW` with `SM_CXSMICON`/`SM_CYSMICON` so the title bar uses the intended small representation. Alternative considered: rely only on `WNDCLASSEXW::hIcon`, but some surfaces can still use a generic or scaled icon.
- List the icon asset in the MSVC project only if Visual Studio project visibility or incremental build behavior requires it. The resource compiler can include the file through `app.rc`, so the project entry is primarily for developer ergonomics.

## Risks / Trade-offs

- Missing or invalid icon file -> resource compilation fails. Mitigation: add the `.ico` asset in the path referenced by `app.rc` and verify with an MSBuild resource compile.
- Poor small-size rendering -> title bar/taskbar icon looks muddy. Mitigation: use a multi-resolution `.ico` containing common Windows sizes, including 16x16 and 32x32.
- Resource ID collision -> wrong resource is loaded. Mitigation: allocate a new `IDI_APPICON` value near existing resource IDs and keep command/control IDs separate.
