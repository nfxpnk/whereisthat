## 1. Scanning Status UI

- [x] 1.1 Implement a modeless scan-progress dialog resource/component that displays scanning state plus file and folder counts.
- [x] 1.2 Route accepted scan start, progress notifications, and scan completion through controller presentation effects so `MainFrame` opens, updates, and closes the dialog.
- [x] 1.3 Add a Cancel action that requests safe scan cancellation, indicates pending cancellation, and discards partial staged output.

## 2. Developer Progress Testing

- [x] 2.1 Add disabled-by-default compile-time per-file delay constants, with a microsecond duration, and ensure enabled test scans expose incremental progress on small sources.

## 3. Verification

- [x] 3.1 Validate the OpenSpec change and build `whereisthat.sln` for `Release|x64`.
- [x] 3.2 Revalidate and rebuild `Release|x64` after adding interactive scan cancellation.
