# `catalog_metadata`

## Purpose And Lifecycle

Contains the single catalog-owned metadata record. `CatalogSchema::Initialize()` inserts row `id = 1` for a new catalog, `Database::SetCatalogDescription()` updates it, and `Database::GetCatalogMetadata()` reads it.

## Keys, Constraints, And Relationships

- Primary key: `id`, constrained to the singleton value `1`.
- No foreign keys or secondary indexes.
- Catalog file size and catalog-wide totals are deliberately not stored here.

## Related Code

| Source | Functions / classes |
|---|---|
| `sql/tables/catalog_metadata.sql`; `src/modules/wit_database/src/Database.cpp` | `CatalogSchema::Initialize`, `SetCatalogDescription`, `GetCatalogMetadata` |
| `src/core/Disk.h` | `CatalogMetadata`, `CatalogSummary` |

## Field Reference

| Field | Type | Nullable | Default | Key/Index | Description | Used in code | Confidence |
|---|---|---:|---|---|---|---|---|
| `id` | `INTEGER` | No | none | `PRIMARY KEY`, `CHECK (id=1)` | Singleton catalog metadata key. | `sql/tables/catalog_metadata.sql`; `CatalogSchema::Initialize` metadata seed | Explicit |
| `description` | `TEXT` | No | `''` | none | Free-text catalog description. | `Database::SetCatalogDescription`, `Database::GetCatalogMetadata` | Explicit |
