# Where Is That?

**Where Is That?** is a Windows desktop app for cataloging files from removable or offline storage.
It is inspired by the classic _Where Is It?_ workflow, but built with modern .NET and WPF.

## Current capabilities

- Create a catalog from any selected folder.
- Persist catalogs and indexed files in a local SQLite database (`catalog.db`).
- Browse previously scanned catalogs without reconnecting the source drive.
- View file name, type, size, path, and last-modified time.

## Project status

This repository is an early but working baseline. The architecture is intentionally simple:

- **UI:** WPF
- **Pattern:** MVVM (CommunityToolkit.Mvvm)
- **Storage:** SQLite (`Microsoft.Data.Sqlite`)
- **Runtime:** .NET 8 (Windows)

## Getting started

### Requirements

- .NET SDK 8.0+
- Windows (WPF target)

### Run

```bash
dotnet run --project whereisthat.csproj
```

### Build

```bash
dotnet build whereisthat.sln
```

## Next improvements

- Search and filtering over indexed files.
- Incremental rescans for existing catalogs.
- Catalog metadata editing (name, notes, tags).
- Import/export and backup functionality.

## Contributing

Contributions are welcome. Small, focused pull requests are preferred.

## License

MIT. See [LICENSE](LICENSE).
