namespace WhereIsThat.Models;

public class FileEntry
{
    public int Id { get; set; }
    public string Path { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;
    public long Size { get; set; }
    public DateTime ModifiedDate { get; set; }
    public string Extension => System.IO.Path.GetExtension(Name);
    public int CatalogId { get; set; }

    public string Type => string.IsNullOrEmpty(Extension) ? "File" : Extension.TrimStart('.').ToUpperInvariant();
    public string DisplaySize => Size switch
    {
        < 1024 => $"{Size} B",
        < 1024 * 1024 => $"{Size / 1024d:F1} KB",
        < 1024 * 1024 * 1024 => $"{Size / (1024d * 1024):F1} MB",
        _ => $"{Size / (1024d * 1024 * 1024):F1} GB"
    };
}
