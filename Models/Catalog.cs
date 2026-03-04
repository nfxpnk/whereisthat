namespace WhereIsThat.Models;

public class Catalog
{
    public int Id { get; set; }
    public string Name { get; set; } = string.Empty;
    public string RootPath { get; set; } = string.Empty;
    public DateTime CreatedDate { get; set; }
    public int ItemCount { get; set; }
}
