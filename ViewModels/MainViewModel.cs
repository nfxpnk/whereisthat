using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using System.ComponentModel;
using WhereIsThat.Models;
using WhereIsThat.Services;

namespace WhereIsThat.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {
        public ObservableCollection<FileEntry> Files { get; set; } = new();
        private readonly FileScanner _scanner = new();
        private readonly DatabaseService _db = new();

        public ICommand ScanCommand { get; set; }

        public MainViewModel()
        {
            ScanCommand = new RelayCommand(Scan);
        }

        private void Scan()
        {
            string folder = "U:\\whereisit"; // Replace with user input
            var catalog = new Catalog { Name = "Scan at " + DateTime.Now, CreatedDate = DateTime.Now };
            int catalogId = _db.AddCatalog(catalog);
            var scannedFiles = _scanner.ScanDirectory(folder, catalogId);
            _db.AddFiles(scannedFiles);
            Files.Clear();
            foreach (var file in scannedFiles) Files.Add(file);
        }

        public event PropertyChangedEventHandler PropertyChanged;
    }

    // Simple RelayCommand implementation
    public class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool> _canExecute;

        public RelayCommand(Action execute, Func<bool> canExecute = null)
        {
            _execute = execute ?? throw new ArgumentNullException(nameof(execute));
            _canExecute = canExecute;
        }

        public bool CanExecute(object parameter) => _canExecute == null || _canExecute();

        public void Execute(object parameter) => _execute();

        public event EventHandler CanExecuteChanged
        {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }
    }
}
