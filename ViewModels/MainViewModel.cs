using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Win32;
using System.Collections.ObjectModel;
using System.Windows;

using WhereIsThat.Models;
using WhereIsThat.Services;

namespace WhereIsThat.ViewModels;

public partial class MainViewModel : ObservableObject
{
    private readonly FileScanner _scanner = new();
    private readonly DatabaseService _db = new();

    [ObservableProperty]
    private Catalog? selectedCatalog;

    public ObservableCollection<Catalog> Catalogs { get; } = new();
    public ObservableCollection<FileEntry> Files { get; } = new();

    public MainViewModel()
    {
        RefreshCatalogs();
    }

    partial void OnSelectedCatalogChanged(Catalog? value)
    {
        Files.Clear();
        if (value is null)
        {
            return;
        }

        foreach (var file in _db.GetFilesByCatalog(value.Id))
        {
            Files.Add(file);
        }
    }

    [RelayCommand]
    private void NewCatalog()
    {
        var dialog = new OpenFolderDialog
        {
            Title = "Select folder to catalog"
        };

        if (dialog.ShowDialog() != true)
        {
            return;
        }

        var path = dialog.FolderName;
        var timestamp = DateTime.Now;
        var catalog = new Catalog
        {
            Name = $"{System.IO.Path.GetFileName(path)} ({timestamp:yyyy-MM-dd HH:mm})",
            RootPath = path,
            CreatedDate = timestamp
        };

        var catalogId = _db.AddCatalog(catalog);
        var files = _scanner.ScanDirectory(path, catalogId);

        _db.AddFiles(files);
        catalog.Id = catalogId;
        catalog.ItemCount = files.Count;

        Catalogs.Insert(0, catalog);
        SelectedCatalog = catalog;
    }

    [RelayCommand]
    private void RefreshCatalogs()
    {
        Catalogs.Clear();
        foreach (var catalog in _db.GetCatalogs())
        {
            Catalogs.Add(catalog);
        }

        SelectedCatalog = Catalogs.FirstOrDefault();
    }

    [RelayCommand]
    private static void Exit()
    {
        Application.Current.Shutdown();
    }
}
