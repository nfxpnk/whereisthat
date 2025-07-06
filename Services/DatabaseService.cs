using Microsoft.Data.Sqlite;
using WhereIsThat.Models;

namespace WhereIsThat.Services {
    public class DatabaseService {
        private readonly string _connectionString = "Data Source=catalog.db";

        public DatabaseService() {
            InitializeDatabase();
        }

        private void InitializeDatabase() {
            using var conn = new SqliteConnection(_connectionString);
            conn.Open();
            var cmd = conn.CreateCommand();
            cmd.CommandText = @"
                CREATE TABLE IF NOT EXISTS Catalogs (
                    Id INTEGER PRIMARY KEY AUTOINCREMENT,
                    Name TEXT,
                    CreatedDate TEXT
                );
                CREATE TABLE IF NOT EXISTS Files (
                    Id INTEGER PRIMARY KEY AUTOINCREMENT,
                    Path TEXT,
                    Name TEXT,
                    Size INTEGER,
                    ModifiedDate TEXT,
                    CatalogId INTEGER
                );";
            cmd.ExecuteNonQuery();
        }

        public int AddCatalog(Catalog catalog) {
            using var conn = new SqliteConnection(_connectionString);
            conn.Open();
            var cmd = conn.CreateCommand();
            cmd.CommandText = @"
                INSERT INTO Catalogs (Name, CreatedDate) VALUES ($name, $date);
                SELECT last_insert_rowid();";
            cmd.Parameters.AddWithValue("$name", catalog.Name);
            cmd.Parameters.AddWithValue("$date", catalog.CreatedDate);
            return (int)(long)cmd.ExecuteScalar();
        }

        public void AddFiles(List<FileEntry> files) {
            using var conn = new SqliteConnection(_connectionString);
            conn.Open();
            foreach (var file in files) {
                var cmd = conn.CreateCommand();
                cmd.CommandText = @"
                    INSERT INTO Files (Path, Name, Size, ModifiedDate, CatalogId) 
                    VALUES ($path, $name, $size, $date, $catalog);";
                cmd.Parameters.AddWithValue("$path", file.Path);
                cmd.Parameters.AddWithValue("$name", file.Name);
                cmd.Parameters.AddWithValue("$size", file.Size);
                cmd.Parameters.AddWithValue("$date", file.ModifiedDate);
                cmd.Parameters.AddWithValue("$catalog", file.CatalogId);
                cmd.ExecuteNonQuery();
            }
        }
    }
}