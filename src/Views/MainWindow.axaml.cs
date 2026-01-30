using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using ApiDetector.ViewModels;
using ApiDetector.Models;

namespace ApiDetector.Views;

public partial class MainWindow : Window
{
    private readonly MainViewModel _viewModel;

    public MainWindow()
    {
        InitializeComponent();
        _viewModel = new MainViewModel();
        DataContext = _viewModel;

        var loadFileButton = this.FindControl<Button>("LoadFileButton");
        loadFileButton?.AddHandler(Button.ClickEvent, OnLoadFileClick);

        var exportButton = this.FindControl<Button>("ExportButton");
        exportButton?.AddHandler(Button.ClickEvent, OnExportClick);
    }

    private async void OnLoadFileClick(object? sender, RoutedEventArgs e)
    {
        var topLevel = GetTopLevel(this);
        if (topLevel == null) return;

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "选择 API Keys 文件",
            AllowMultiple = false,
            FileTypeFilter =
            [
                new FilePickerFileType("文本文件") { Patterns = ["*.txt"] },
                new FilePickerFileType("所有文件") { Patterns = ["*.*"] }
            ]
        });

        if (files.Count > 0)
        {
            var file = files[0];
            await using var stream = await file.OpenReadAsync();
            using var reader = new StreamReader(stream);
            _viewModel.InputKeys = await reader.ReadToEndAsync();
        }
    }

    private async void OnExportClick(object? sender, RoutedEventArgs e)
    {
        var topLevel = GetTopLevel(this);
        if (topLevel == null) return;

        var validKeys = _viewModel.Results
            .Where(r => r.Status == KeyStatus.Valid)
            .Select(r => r.Key)
            .ToList();

        if (validKeys.Count == 0)
        {
            _viewModel.StatusText = "没有有效的 Key 可导出";
            return;
        }

        var file = await topLevel.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "导出有效 Keys",
            SuggestedFileName = "valid_keys.txt",
            FileTypeChoices =
            [
                new FilePickerFileType("文本文件") { Patterns = ["*.txt"] }
            ]
        });

        if (file != null)
        {
            await using var stream = await file.OpenWriteAsync();
            await using var writer = new StreamWriter(stream);
            foreach (var key in validKeys)
            {
                await writer.WriteLineAsync(key);
            }
            _viewModel.StatusText = $"已导出 {validKeys.Count} 个有效 Key";
        }
    }
}
