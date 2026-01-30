namespace ApiDetector.Models;

/// <summary>
/// API Key 状态枚举
/// </summary>
public enum KeyStatus
{
    Pending,
    Valid,
    Invalid,
    Error
}

/// <summary>
/// 单个 Key 检测结果
/// </summary>
public record KeyCheckResult
{
    public required string Key { get; init; }
    public KeyStatus Status { get; init; }
    public string? Message { get; init; }
    public int LatencyMs { get; init; }
    public int HttpStatusCode { get; init; }
    public DateTime CheckedAt { get; init; } = DateTime.UtcNow;

    public string MaskedKey => Key.Length <= 8
        ? "****"
        : $"{Key[..4]}...{Key[^4..]}";

    public string StatusIcon => Status switch
    {
        KeyStatus.Valid => "✅",
        KeyStatus.Invalid => "❌",
        KeyStatus.Error => "⚠️",
        _ => "⏳"
    };
}

/// <summary>
/// 检测统计信息
/// </summary>
public class CheckStats
{
    public int Total { get; set; }
    public int Checked { get; set; }
    public int Valid { get; set; }
    public int Invalid { get; set; }
    public int Error { get; set; }
    public double DurationSeconds { get; set; }
    public double KeysPerSecond => DurationSeconds > 0 ? Checked / DurationSeconds : 0;
    public double Progress => Total > 0 ? (double)Checked / Total * 100 : 0;
}

/// <summary>
/// 检测器配置
/// </summary>
public record DetectorConfig
{
    public string BaseUrl { get; set; } = "https://api.openai.com/v1/models";
    public int Concurrent { get; set; } = 100;
    public int TimeoutSeconds { get; set; } = 10;
    public string HttpMethod { get; set; } = "GET";
    public Dictionary<string, string>? CustomHeaders { get; set; }
}

/// <summary>
/// API 预设配置
/// </summary>
public record ApiPreset
{
    public required string Name { get; init; }
    public required string Url { get; init; }
    public string Method { get; init; } = "GET";
    public Dictionary<string, string>? Headers { get; init; }

    public static readonly List<ApiPreset> BuiltInPresets =
    [
        new() { Name = "OpenAI", Url = "https://api.openai.com/v1/models" },
        new() { Name = "Anthropic Claude", Url = "https://api.anthropic.com/v1/messages", Method = "POST",
                Headers = new() { ["anthropic-version"] = "2023-06-01" } },
        new() { Name = "Google Gemini", Url = "https://generativelanguage.googleapis.com/v1beta/models" },
        new() { Name = "Groq", Url = "https://api.groq.com/openai/v1/models" },
        new() { Name = "Mistral", Url = "https://api.mistral.ai/v1/models" },
        new() { Name = "DeepSeek", Url = "https://api.deepseek.com/v1/models" },
        new() { Name = "OpenRouter", Url = "https://openrouter.ai/api/v1/models" },
        new() { Name = "自定义", Url = "" }
    ];
}
