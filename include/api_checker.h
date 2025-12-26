#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <unordered_set>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace api_checker {

enum class KeyStatus {
    Valid,
    Invalid,
    Error,
    Pending  // 新增：待检测状态
};

struct KeyResult {
    std::string key;
    KeyStatus status;
    std::string message;
    std::chrono::system_clock::time_point checked_at;
    std::optional<std::chrono::milliseconds> response_time;

    nlohmann::json to_json() const;
};

struct CheckStats {
    size_t total = 0;
    std::atomic<size_t> checked{0};
    std::atomic<size_t> valid{0};
    std::atomic<size_t> invalid{0};
    std::atomic<size_t> error{0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    double duration_secs = 0.0;
    double avg_speed = 0.0;
    size_t concurrent_used = 0;
    size_t timeout_used = 0;

    nlohmann::json to_json() const;
};

struct CheckResults {
    CheckStats stats;
    std::vector<KeyResult> valid_keys;
    std::vector<KeyResult> invalid_keys;
    std::vector<KeyResult> error_keys;

    nlohmann::json to_json() const;
};

struct CheckProgress {
    std::string session_id;
    std::string input_file;
    std::vector<std::string> all_keys;
    std::vector<KeyResult> completed_results;
    std::unordered_set<std::string> processed_keys;
    CheckStats stats;
    std::chrono::system_clock::time_point last_save_time;
    size_t concurrent_used = 1000;
    size_t timeout_used = 10;

    nlohmann::json to_json() const;
    static CheckProgress from_json(const nlohmann::json& j);

    // 获取未处理的keys
    std::vector<std::string> get_pending_keys() const;

    // 检查是否已处理某个key
    bool is_key_processed(const std::string& key) const;
};

class APIKeyChecker {
public:
    APIKeyChecker(size_t timeout_secs = 10, size_t connect_timeout = 5,
                  size_t concurrent = 1000);
    ~APIKeyChecker();

    // 检测单个API Key
    std::future<KeyResult> check_single_key_async(const std::string& api_key);

    // 批量检测API Keys
    CheckResults check_keys(const std::vector<std::string>& api_keys,
                           size_t concurrent = 1000,
                           bool quiet = false);

    // 带进度保存的批量检测
    CheckResults check_keys_with_progress(const std::vector<std::string>& api_keys,
                                         const std::string& input_file,
                                         size_t concurrent = 1000,
                                         bool quiet = false);

    // 从进度文件恢复检测
    CheckResults resume_from_progress(const std::string& progress_file,
                                     bool quiet = false);

    // 查找最新的进度文件
    static std::optional<std::string> find_latest_progress_file(const std::string& input_file);

    // 停止检测
    void stop();

    // 获取当前统计
    const CheckStats& get_stats() const { return stats_; }

    // 保存当前进度
    bool save_progress(const CheckProgress& progress, const std::string& progress_file = "");

    // 加载进度文件
    static std::optional<CheckProgress> load_progress(const std::string& progress_file);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    CheckStats stats_;
    std::atomic<bool> should_stop_{false};

    // 进度保存相关
    std::string current_session_id_;
    std::string current_progress_file_;
    std::chrono::steady_clock::time_point last_save_time_;
    static constexpr std::chrono::seconds SAVE_INTERVAL{30}; // 每30秒保存一次

    // 内部检测方法（支持进度保存）
    CheckResults check_keys_internal(const std::vector<std::string>& api_keys,
                                   size_t concurrent, bool quiet,
                                   CheckProgress* progress);
};

} // namespace api_checker