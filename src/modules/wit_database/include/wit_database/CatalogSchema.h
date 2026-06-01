#pragma once

namespace wit::storage {
class SqliteConnection;

class CatalogSchema {
public:
    static bool Initialize(SqliteConnection& connection);
    static bool Validate(SqliteConnection& connection);
};
}
