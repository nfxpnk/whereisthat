# `catalog_metadata`

## Purpose And Lifecycle

Contains the single catalog-owned metadata record. `Database::InitializeSchema()` inserts row `id = 1` for a new catalog, `Database::SetCatalogDescription()` updates it, and `Database::GetCatalogMetadata()` reads it.

## Keys, Constraints, And Relationships

- Primary key: `id`, constrained to the singleton value `1`.
- No foreign keys or secondary indexes.
- Catalog file size and catalog-wide totals are deliberately not stored here.

## Related Code

| Source | Functions / classes |
|---|---|
| `src/storage/Database.cpp` | `InitializeSchema`, `SetCatalogDescription`, `GetCatalogMetadata` |
| `src/core/Disk.h` | `CatalogMetadata`, `CatalogSummary` |

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY`, `CHECK (id=1)` | Singleton catalog metadata key. | `src/storage/Database.cpp`: initialization and description reads/writes | Explicit |
| `description` | `TEXT` | No | `''` | none | Free-text catalog description. | `src/storage/Database.cpp`: `SetCatalogDescription`, `GetCatalogMetadata` | Explicit |
