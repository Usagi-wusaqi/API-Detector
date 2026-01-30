using System.Collections.Concurrent;
using System.Diagnostics;
using ApiDetector.Models;

namespace ApiDetector.Services;

/// <summary>
/// API Key 检测器核心服务
/// </summary>
public class ApiKeyDetector : IDisposable
{
    private readonly HttpClient _httpClient;
    private readonly DetectorConfig _config;
    private readonly SemaphoreSlim _semaphore;
    private CancellationTokenSource? _cts;

    public ApiKeyDetector(DetectorConfig config)
    {
        _config = config;
        _semaphore = new SemaphoreSlim(config.Concurrent, config.Concurrent);

        var handler = new SocketsHttpHandler
        {
            PooledConnectionLifetime = TimeSpan.FromMinutes(2),
            MaxConnectionsPerServer = config.Concurrent,
            EnableMultipleHttp2Connections = true,
            ConnectTimeout = TimeSpan.FromSeconds(config.TimeoutSeconds)
        };

        _httpClient = new HttpClient(handler)
        {
            Timeout = TimeSpan.FromSeconds(config.TimeoutSeconds)
        };

        _httpClient.DefaultRequestHeaders.Add("User-Agent", "ApiDetector/2.0");
    }

    /// <summary>
    /// 批量检测 API Keys
    /// </summary>
    public async Task<List<KeyCheckResult>> CheckKeysAsync(
        List<string> keys,
        Action<KeyCheckResult>? onProgress = null,
        CancellationToken cancellationToken = default)
    {
        _cts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        var results = new ConcurrentBag<KeyCheckResult>();

        var tasks = keys.Select(async key =>
        {
            if (_cts.Token.IsCancellationRequested) return;

            await _semaphore.WaitAsync(_cts.Token);
            try
            {
                if (_cts.Token.IsCancellationRequested) return;

                var result = await CheckSingleKeyAsync(key, _cts.Token);
                results.Add(result);
                onProgress?.Invoke(result);
            }
            finally
            {
                _semaphore.Release();
            }
        });

        try
        {
            await Task.WhenAll(tasks);
        }
        catch (OperationCanceledException)
        {
            // 用户取消，返回已检测的结果
        }

        return [.. results];
    }

    /// <summary>
    /// 取消检测
    /// </summary>
    public void Cancel()
    {
        _cts?.Cancel();
    }

    /// <summary>
    /// 检测单个 API Key
    /// </summary>
    public async Task<KeyCheckResult> CheckSingleKeyAsync(
        string key,
        CancellationToken cancellationToken = default)
    {
        var sw = Stopwatch.StartNew();

        try
        {
            using var request = new HttpRequestMessage(
                _config.HttpMethod.ToUpperInvariant() switch
                {
                    "POST" => HttpMethod.Post,
                    "PUT" => HttpMethod.Put,
                    "DELETE" => HttpMethod.Delete,
                    "PATCH" => HttpMethod.Patch,
                    _ => HttpMethod.Get
                },
                _config.BaseUrl);

            request.Headers.Add("Authorization", $"Bearer {key}");

            if (_config.CustomHeaders != null)
            {
                foreach (var (headerKey, headerValue) in _config.CustomHeaders)
                {
                    request.Headers.TryAddWithoutValidation(headerKey, headerValue);
                }
            }

            using var response = await _httpClient.SendAsync(request, cancellationToken);
            sw.Stop();

            var statusCode = (int)response.StatusCode;

            var status = statusCode switch
            {
                >= 200 and < 300 => KeyStatus.Valid,
                401 or 403 => KeyStatus.Invalid,
                429 => KeyStatus.Valid,
                _ => KeyStatus.Error
            };

            var message = statusCode switch
            {
                200 or 201 => "OK",
                401 => "无效 Key",
                403 => "无权限",
                429 => "有效 (限流)",
                404 => "端点不存在",
                500 => "服务器错误",
                502 => "网关错误",
                503 => "服务不可用",
                _ => $"HTTP {statusCode}"
            };

            return new KeyCheckResult
            {
                Key = key,
                Status = status,
                Message = message,
                LatencyMs = (int)sw.ElapsedMilliseconds,
                HttpStatusCode = statusCode
            };
        }
        catch (TaskCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            return new KeyCheckResult
            {
                Key = key,
                Status = KeyStatus.Error,
                Message = "超时",
                LatencyMs = (int)sw.ElapsedMilliseconds,
                HttpStatusCode = 0
            };
        }
        catch (OperationCanceledException)
        {
            return new KeyCheckResult
            {
                Key = key,
                Status = KeyStatus.Pending,
                Message = "已取消",
                LatencyMs = (int)sw.ElapsedMilliseconds,
                HttpStatusCode = 0
            };
        }
        catch (HttpRequestException)
        {
            return new KeyCheckResult
            {
                Key = key,
                Status = KeyStatus.Error,
                Message = "网络错误",
                LatencyMs = (int)sw.ElapsedMilliseconds,
                HttpStatusCode = 0
            };
        }
        catch
        {
            return new KeyCheckResult
            {
                Key = key,
                Status = KeyStatus.Error,
                Message = "未知错误",
                LatencyMs = (int)sw.ElapsedMilliseconds,
                HttpStatusCode = 0
            };
        }
    }

    public void Dispose()
    {
        _cts?.Dispose();
        _httpClient.Dispose();
        _semaphore.Dispose();
        GC.SuppressFinalize(this);
    }
}
