#pragma once

namespace wit::storage {
class SqliteConnection;

class CatalogSchema {
public:
    [[nodiscard]] static bool Initialize(SqliteConnection& connection);
    [[nodiscard]] static bool Validate(SqliteConnection& connection);
};
}
