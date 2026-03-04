using Microsoft.Data.Sqlite;
using WhereIsThat.Models;

namespace WhereIsThat.Services;

public class DatabaseService
{
    private readonly string _connectionString = "Data Source=catalog.db";

    public DatabaseService()
    {
        InitializeDatabase();
    }

    private void InitializeDatabase()
    {
        using var conn = new SqliteConnection(_connectionString);
        conn.Open();
        var cmd = conn.CreateCommand();
        cmd.CommandText = @"
            CREATE TABLE IF NOT EXISTS Catalogs (
                Id INTEGER PRIMARY KEY AUTOINCREMENT,
                Name TEXT NOT NULL,
                RootPath TEXT NOT NULL,
                CreatedDate TEXT NOT NULL,
                ItemCount INTEGER NOT NULL DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS Files (
                Id INTEGER PRIMARY KEY AUTOINCREMENT,
                Path TEXT NOT NULL,
                Name TEXT NOT NULL,
                Size INTEGER NOT NULL,
                ModifiedDate TEXT NOT NULL,
                CatalogId INTEGER NOT NULL,
                FOREIGN KEY (CatalogId) REFERENCES Catalogs(Id)
            );";
        cmd.ExecuteNonQuery();
    }

    public int AddCatalog(Catalog catalog)
    {
        using var conn = new SqliteConnection(_connectionString);
        conn.Open();
        var cmd = conn.CreateCommand();
        cmd.CommandText = @"
            INSERT INTO Catalogs (Name, RootPath, CreatedDate, ItemCount)
            VALUES ($name, $rootPath, $date, $itemCount);
            SELECT last_insert_rowid();";
        cmd.Parameters.AddWithValue("$name", catalog.Name);
        cmd.Parameters.AddWithValue("$rootPath", catalog.RootPath);
        cmd.Parameters.AddWithValue("$date", catalog.CreatedDate.ToString("O"));
        cmd.Parameters.AddWithValue("$itemCount", catalog.ItemCount);

        return (int)(long)cmd.ExecuteScalar()!;
    }

    public void AddFiles(IReadOnlyCollection<FileEntry> files)
    {
        using var conn = new SqliteConnection(_connectionString);
        conn.Open();

        using var transaction = conn.BeginTransaction();
        foreach (var file in files)
        {
            var cmd = conn.CreateCommand();
            cmd.Transaction = transaction;
            cmd.CommandText = @"
                INSERT INTO Files (Path, Name, Size, ModifiedDate, CatalogId) 
                VALUES ($path, $name, $size, $date, $catalog);";
            cmd.Parameters.AddWithValue("$path", file.Path);
            cmd.Parameters.AddWithValue("$name", file.Name);
            cmd.Parameters.AddWithValue("$size", file.Size);
            cmd.Parameters.AddWithValue("$date", file.ModifiedDate.ToString("O"));
            cmd.Parameters.AddWithValue("$catalog", file.CatalogId);
            cmd.ExecuteNonQuery();
        }

        transaction.Commit();
    }

    public List<Catalog> GetCatalogs()
    {
        using var conn = new SqliteConnection(_connectionString);
        conn.Open();

        var cmd = conn.CreateCommand();
        cmd.CommandText = "SELECT Id, Name, RootPath, CreatedDate, ItemCount FROM Catalogs ORDER BY CreatedDate DESC";

        var results = new List<Catalog>();
        using var reader = cmd.ExecuteReader();
        while (reader.Read())
        {
            results.Add(new Catalog
            {
                Id = reader.GetInt32(0),
                Name = reader.GetString(1),
                RootPath = reader.GetString(2),
                CreatedDate = DateTime.Parse(reader.GetString(3)),
                ItemCount = reader.GetInt32(4)
            });
        }

        return results;
    }

    public List<FileEntry> GetFilesByCatalog(int catalogId)
    {
        using var conn = new SqliteConnection(_connectionString);
        conn.Open();

        var cmd = conn.CreateCommand();
        cmd.CommandText = @"
            SELECT Id, Path, Name, Size, ModifiedDate, CatalogId
            FROM Files
            WHERE CatalogId = $catalogId
            ORDER BY Path, Name";
        cmd.Parameters.AddWithValue("$catalogId", catalogId);

        var results = new List<FileEntry>();
        using var reader = cmd.ExecuteReader();
        while (reader.Read())
        {
            results.Add(new FileEntry
            {
                Id = reader.GetInt32(0),
                Path = reader.GetString(1),
                Name = reader.GetString(2),
                Size = reader.GetInt64(3),
                ModifiedDate = DateTime.Parse(reader.GetString(4)),
                CatalogId = reader.GetInt32(5)
            });
        }

        return results;
    }
}
