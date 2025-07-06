namespace WhereIsThat.Models {
    public class FileEntry {
        public int Id { get; set; }
        public string Path { get; set; }
        public string Name { get; set; }
        public long Size { get; set; }
        public DateTime ModifiedDate { get; set; }
        public string Extension => System.IO.Path.GetExtension(Name);
        public int CatalogId { get; set; }
    }
}