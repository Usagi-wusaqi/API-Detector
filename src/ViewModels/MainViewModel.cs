using System.Collections.ObjectModel;
using System.Diagnostics;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using ApiDetector.Models;
using ApiDetector.Services;

namespace ApiDetector.ViewModels;

public partial class MainViewModel : ObservableObject
{
    [ObservableProperty]
    private string _inputKeys = string.Empty;

    [ObservableProperty]
    private string _apiUrl = "https://api.openai.com/v1/models";

    [ObservableProperty]
    private int _concurrent = 100;

    [ObservableProperty]
    private int _timeout = 10;

    [ObservableProperty]
    private ApiPreset? _selectedPreset;

    [ObservableProperty]
    private bool _isRunning;

    [ObservableProperty]
    private double _progress;

    [ObservableProperty]
    private string _statusText = "就绪";

    [ObservableProperty]
    private CheckStats _stats = new();

    public ObservableCollection<KeyCheckResult> Results { get; } = [];

    public List<ApiPreset> Presets => ApiPreset.BuiltInPresets;

    private ApiKeyDetector? _detector;
    private readonly Stopwatch _stopwatch = new();

    public MainViewModel()
    {
        SelectedPreset = Presets[0];
    }

    partial void OnSelectedPresetChanged(ApiPreset? value)
    {
        if (value != null && !string.IsNullOrEmpty(value.Url))
        {
            ApiUrl = value.Url;
        }
    }

    [RelayCommand(CanExecute = nameof(CanStartCheck))]
    private async Task StartCheck()
    {
        var keys = InputKeys
            .Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries)
            .Select(k => k.Trim())
            .Where(k => !string.IsNullOrWhiteSpace(k))
            .Distinct()
            .ToList();

        if (keys.Count == 0)
        {
            StatusText = "请输入 API Keys";
            return;
        }

        IsRunning = true;
        Results.Clear();
        Progress = 0;
        Stats = new CheckStats { Total = keys.Count };
        _stopwatch.Restart();

        var config = new DetectorConfig
        {
            BaseUrl = ApiUrl,
            Concurrent = Concurrent,
            TimeoutSeconds = Timeout,
            HttpMethod = SelectedPreset?.Method ?? "GET",
            CustomHeaders = SelectedPreset?.Headers
        };

        _detector = new ApiKeyDetector(config);

        StatusText = $"正在检测 {keys.Count} 个 Keys...";

        try
        {
            await _detector.CheckKeysAsync(keys, OnKeyChecked);
        }
        finally
        {
            _stopwatch.Stop();
            _detector.Dispose();
            _detector = null;
            IsRunning = false;

            Stats.DurationSeconds = _stopwatch.Elapsed.TotalSeconds;
            StatusText = $"完成! 有效: {Stats.Valid}, 无效: {Stats.Invalid}, 错误: {Stats.Error}, 耗时: {Stats.DurationSeconds:F1}s";
        }
    }

    private bool CanStartCheck() => !IsRunning && !string.IsNullOrWhiteSpace(InputKeys);

    [RelayCommand(CanExecute = nameof(CanStopCheck))]
    private void StopCheck()
    {
        _detector?.Cancel();
        StatusText = "正在停止...";
    }

    private bool CanStopCheck() => IsRunning;

    [RelayCommand]
    private void ClearResults()
    {
        Results.Clear();
        Stats = new CheckStats();
        Progress = 0;
        StatusText = "已清空";
    }

    private void OnKeyChecked(KeyCheckResult result)
    {
        Avalonia.Threading.Dispatcher.UIThread.Post(() =>
        {
            Results.Add(result);

            Stats.Checked++;
            switch (result.Status)
            {
                case KeyStatus.Valid:
                    Stats.Valid++;
                    break;
                case KeyStatus.Invalid:
                    Stats.Invalid++;
                    break;
                case KeyStatus.Error:
                    Stats.Error++;
                    break;
            }

            Progress = Stats.Progress;
            Stats.DurationSeconds = _stopwatch.Elapsed.TotalSeconds;

            OnPropertyChanged(nameof(Stats));
        });
    }
}
