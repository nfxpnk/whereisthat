using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Windows;
using System.Windows.Controls;

namespace WhereIsThat
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            LoadRootCatalogs();
        }

        private void LoadRootCatalogs()
        {
            // Load drives as root catalogs
            // foreach (var drive in DriveInfo.GetDrives())
            // {
            //     if (drive.IsReady)
            //     {
            //         var rootItem = new TreeViewItem
            //         {
            //             Header = drive.Name,
            //             Tag = drive.RootDirectory.FullName
            //         };

            //         // Add dummy child to show expand arrow
            //         rootItem.Items.Add(null);
            //         rootItem.Expanded += Folder_Expanded;

            //         CatalogTreeView.Items.Add(rootItem);
            //     }
            // }
        }

        private void Folder_Expanded(object sender, RoutedEventArgs e)
        {
            // var item = (TreeViewItem)sender;
            // if (item.Items.Count == 1 && item.Items[0] == null)
            // {
            //     item.Items.Clear();

            //     string path = (string)item.Tag;
            //     try
            //     {
            //         foreach (var dir in Directory.GetDirectories(path))
            //         {
            //             var subItem = new TreeViewItem
            //             {
            //                 Header = System.IO.Path.GetFileName(dir),
            //                 Tag = dir
            //             };
            //             subItem.Items.Add(null); // dummy child
            //             subItem.Expanded += Folder_Expanded;
            //             item.Items.Add(subItem);
            //         }
            //     }
            //     catch
            //     {
            //         // Ignore access exceptions
            //     }
            // }
        }

        private void CatalogTreeView_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
        {
            // CatalogListView.Items.Clear();

            // if (CatalogTreeView.SelectedItem is TreeViewItem selectedItem)
            // {
            //     string path = (string)selectedItem.Tag;

            //     try
            //     {
            //         // List folders
            //         foreach (var dir in Directory.GetDirectories(path))
            //         {
            //             var info = new DirectoryInfo(dir);
            //             CatalogListView.Items.Add(new CatalogItem
            //             {
            //                 Name = info.Name,
            //                 Type = "Folder",
            //                 Size = ""
            //             });
            //         }

            //         // List files
            //         foreach (var file in Directory.GetFiles(path))
            //         {
            //             var info = new FileInfo(file);
            //             CatalogListView.Items.Add(new CatalogItem
            //             {
            //                 Name = info.Name,
            //                 Type = "File",
            //                 Size = $"{info.Length / 1024} KB"
            //             });
            //         }
            //     }
            //     catch
            //     {
            //         // Ignore access exceptions
            //     }
            // }
        }

        private void Menu_New_Click(object sender, RoutedEventArgs e)
        {
            // Clear TreeView and ListView
            CatalogTreeView.Items.Clear();
            CatalogListView.Items.Clear();
            LoadRootCatalogs();
        }

        private void Menu_Open_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Open clicked - implement catalog loading");
        }

        private void Menu_Save_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Save clicked - implement catalog saving");
        }

        private void Menu_Close_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }

    public class CatalogItem
    {
        public string Name { get; set; }
        public string Type { get; set; }
        public string Size { get; set; }
    }
}
