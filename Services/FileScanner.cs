using System.IO;
using WhereIsThat.Models;

namespace WhereIsThat.Services;

public class FileScanner
{
    public List<FileEntry> ScanDirectory(string rootPath, int catalogId)
    {
        var files = new List<FileEntry>();

        foreach (var filePath in EnumerateFilesSafe(rootPath))
        {
            try
            {
                var info = new FileInfo(filePath);
                files.Add(new FileEntry
                {
                    Name = info.Name,
                    Path = info.DirectoryName ?? rootPath,
                    Size = info.Length,
                    ModifiedDate = info.LastWriteTime,
                    CatalogId = catalogId
                });
            }
            catch
            {
                // Ignore individual files that cannot be read.
            }
        }

        return files;
    }

    private static IEnumerable<string> EnumerateFilesSafe(string rootPath)
    {
        var directories = new Stack<string>();
        directories.Push(rootPath);

        while (directories.Count > 0)
        {
            var current = directories.Pop();

            IEnumerable<string> subDirectories = Array.Empty<string>();
            IEnumerable<string> files = Array.Empty<string>();

            try
            {
                subDirectories = Directory.EnumerateDirectories(current);
            }
            catch
            {
                // Skip inaccessible directories.
            }

            try
            {
                files = Directory.EnumerateFiles(current);
            }
            catch
            {
                // Skip file enumeration errors.
            }

            foreach (var subDirectory in subDirectories)
            {
                directories.Push(subDirectory);
            }

            foreach (var file in files)
            {
                yield return file;
            }
        }
    }
}
