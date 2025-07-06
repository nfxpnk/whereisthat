using WhereIsThat.Models;
using System.IO;

namespace WhereIsThat.Services {
    public class FileScanner {
        public List<FileEntry> ScanDirectory(string rootPath, int catalogId) {
            var files = Directory.GetFiles(rootPath, "*.*", SearchOption.AllDirectories);
            return files.Select(file => {
                var info = new FileInfo(file);
                return new FileEntry {
                    Name = info.Name,
                    Path = info.DirectoryName,
                    Size = info.Length,
                    ModifiedDate = info.LastWriteTime,
                    CatalogId = catalogId
                };
            }).ToList();
        }
    }
}