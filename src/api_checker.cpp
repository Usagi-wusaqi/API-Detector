#include "api_checker.h"
#include "http_client.h"
#include "progress_bar.h"
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <optional>

namespace api_checker {

// KeyResult JSON序列化
nlohmann::json KeyResult::to_json() const {
    nlohmann::json j;
    j["key"] = key;
    j["status"] = (status == KeyStatus::Valid) ? "valid" :
                  (status == KeyStatus::Invalid) ? "invalid" :
                  (status == KeyStatus::Error) ? "error" : "pending";
    j["message"] = message;

    auto time_t = std::chrono::system_clock::to_time_t(checked_at);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    j["checked_at"] = ss.str();

    if (response_time) {
        j["response_time_ms"] = response_time->count();
    }

    return j;
}

// CheckStats JSON序列化
nlohmann::json CheckStats::to_json() const {
    nlohmann::json j;
    j["total"] = total;
    j["checked"] = checked.load();
    j["valid"] = valid.load();
    j["invalid"] = invalid.load();
    j["error"] = error.load();

    auto start_time_t = std::chrono::system_clock::to_time_t(start_time);
    auto end_time_t = std::chrono::system_clock::to_time_t(end_time);

    std::stringstream ss1, ss2;
    ss1 << std::put_time(std::gmtime(&start_time_t), "%Y-%m-%dT%H:%M:%SZ");
    ss2 << std::put_time(std::gmtime(&end_time_t), "%Y-%m-%dT%H:%M:%SZ");

    j["start_time"] = ss1.str();
    j["end_time"] = ss2.str();
    j["duration_secs"] = duration_secs;
    j["avg_speed"] = avg_speed;
    j["concurrent_used"] = concurrent_used;
    j["timeout_used"] = timeout_used;

    return j;
}

// CheckResults JSON序列化
nlohmann::json CheckResults::to_json() const {
    nlohmann::json j;
    j["stats"] = stats.to_json();

    j["valid_keys"] = nlohmann::json::array();
    for (const auto& result : valid_keys) {
        j["valid_keys"].push_back(result.to_json());
    }

    j["invalid_keys"] = nlohmann::json::array();
    for (const auto& result : invalid_keys) {
        j["invalid_keys"].push_back(result.to_json());
    }

    j["error_keys"] = nlohmann::json::array();
    for (const auto& result : error_keys) {
        j["error_keys"].push_back(result.to_json());
    }

    return j;
}

// CheckProgress JSON序列化
nlohmann::json CheckProgress::to_json() const {
    nlohmann::json j;
    j["session_id"] = session_id;
    j["input_file"] = input_file;
    j["all_keys"] = all_keys;
    j["concurrent_used"] = concurrent_used;
    j["timeout_used"] = timeout_used;

    j["completed_results"] = nlohmann::json::array();
    for (const auto& result : completed_results) {
        j["completed_results"].push_back(result.to_json());
    }

    j["processed_keys"] = nlohmann::json::array();
    for (const auto& key : processed_keys) {
        j["processed_keys"].push_back(key);
    }

    j["stats"] = stats.to_json();

    auto time_t = std::chrono::system_clock::to_time_t(last_save_time);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    j["last_save_time"] = ss.str();

    return j;
}

// CheckProgress JSON反序列化
CheckProgress CheckProgress::from_json(const nlohmann::json& j) {
    CheckProgress progress;
    progress.session_id = j["session_id"];
    progress.input_file = j["input_file"];
    progress.all_keys = j["all_keys"];
    progress.concurrent_used = j["concurrent_used"];
    progress.timeout_used = j["timeout_used"];

    for (const auto& result_json : j["completed_results"]) {
        KeyResult result;
        result.key = result_json["key"];

        std::string status_str = result_json["status"];
        if (status_str == "valid") result.status = KeyStatus::Valid;
        else if (status_str == "invalid") result.status = KeyStatus::Invalid;
        else if (status_str == "error") result.status = KeyStatus::Error;
        else result.status = KeyStatus::Pending;

        result.message = result_json["message"];

        // 解析时间
        std::string time_str = result_json["checked_at"];
        std::tm tm = {};
        std::istringstream ss(time_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        result.checked_at = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        if (result_json.contains("response_time_ms")) {
            result.response_time = std::chrono::milliseconds(result_json["response_time_ms"]);
        }

        progress.completed_results.push_back(result);
    }

    for (const auto& key : j["processed_keys"]) {
        progress.processed_keys.insert(key);
    }

    // 解析统计信息
    if (j.contains("stats")) {
        const auto& stats_json = j["stats"];
        progress.stats.total = stats_json["total"];
        progress.stats.checked = stats_json["checked"];
        progress.stats.valid = stats_json["valid"];
        progress.stats.invalid = stats_json["invalid"];
        progress.stats.error = stats_json["error"];
        progress.stats.concurrent_used = stats_json["concurrent_used"];
        progress.stats.timeout_used = stats_json["timeout_used"];

        // 解析时间
        std::string start_time_str = stats_json["start_time"];
        std::tm start_tm = {};
        std::istringstream start_ss(start_time_str);
        start_ss >> std::get_time(&start_tm, "%Y-%m-%dT%H:%M:%SZ");
        progress.stats.start_time = std::chrono::system_clock::from_time_t(std::mktime(&start_tm));
    }

    // 解析保存时间
    std::string save_time_str = j["last_save_time"];
    std::tm save_tm = {};
    std::istringstream save_ss(save_time_str);
    save_ss >> std::get_time(&save_tm, "%Y-%m-%dT%H:%M:%SZ");
    progress.last_save_time = std::chrono::system_clock::from_time_t(std::mktime(&save_tm));

    return progress;
}

// 获取未处理的keys
std::vector<std::string> CheckProgress::get_pending_keys() const {
    std::vector<std::string> pending;
    for (const auto& key : all_keys) {
        if (processed_keys.find(key) == processed_keys.end()) {
            pending.push_back(key);
        }
    }
    return pending;
}

// 检查是否已处理某个key
bool CheckProgress::is_key_processed(const std::string& key) const {
    return processed_keys.find(key) != processed_keys.end();
}

// APIKeyChecker实现
class APIKeyChecker::Impl {
public:
    Impl(size_t timeout_secs, size_t connect_timeout)
        : timeout_secs_(timeout_secs), connect_timeout_(connect_timeout) {

        // 初始化HTTP客户端
        http_client_.set_timeout(std::chrono::seconds(timeout_secs));
        http_client_.set_connect_timeout(std::chrono::seconds(connect_timeout));
        http_client_.set_user_agent("api-key-checker/1.0");
    }

    KeyResult check_single_key(const std::string& api_key) {
        auto start_time = std::chrono::steady_clock::now();
        auto checked_at = std::chrono::system_clock::now();

        std::string trimmed_key = api_key;
        // 去除首尾空白字符
        trimmed_key.erase(0, trimmed_key.find_first_not_of(" \t\r\n"));
        trimmed_key.erase(trimmed_key.find_last_not_of(" \t\r\n") + 1);

        if (trimmed_key.empty()) {
            return {trimmed_key, KeyStatus::Error, "空 key", checked_at, std::nullopt};
        }

        if (trimmed_key.length() < 3 || trimmed_key.substr(0, 3) != "sk-") {
            return {trimmed_key, KeyStatus::Error, "无效格式 (必须以 sk- 开头)",
                   checked_at, std::nullopt};
        }

        // 发送HTTP请求
        std::vector<std::string> headers = {
            "Authorization: Bearer " + trimmed_key,
            "Content-Type: application/json"
        };

        auto response = http_client_.get("https://api.openai.com/v1/models", headers);
        auto end_time = std::chrono::steady_clock::now();
        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        if (!response.success) {
            return {trimmed_key, KeyStatus::Error, "请求错误: " + response.error_message,
                   checked_at, response_time};
        }

        switch (response.status_code) {
            case 200:
                return {trimmed_key, KeyStatus::Valid, "有效", checked_at, response_time};
            case 401:
                return {trimmed_key, KeyStatus::Invalid, "认证失败", checked_at, response_time};
            case 403:
                return {trimmed_key, KeyStatus::Invalid, "访问被拒绝", checked_at, response_time};
            case 429:
                return {trimmed_key, KeyStatus::Error, "请求过多，稍后重试",
                       checked_at, response_time};
            default:
                if (response.status_code >= 500) {
                    return {trimmed_key, KeyStatus::Error,
                           "服务器错误 " + std::to_string(response.status_code),
                           checked_at, response_time};
                } else {
                    return {trimmed_key, KeyStatus::Invalid,
                           "HTTP " + std::to_string(response.status_code),
                           checked_at, response_time};
                }
        }
    }

private:
    HttpClient http_client_;
    size_t timeout_secs_;
    size_t connect_timeout_;
};

APIKeyChecker::APIKeyChecker(size_t timeout_secs, size_t connect_timeout, size_t concurrent)
    : pImpl_(std::make_unique<Impl>(timeout_secs, connect_timeout)) {

    stats_.concurrent_used = concurrent;
    stats_.timeout_used = timeout_secs;

    // 生成会话ID
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "session_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    current_session_id_ = ss.str();

    last_save_time_ = std::chrono::steady_clock::now();
}

APIKeyChecker::~APIKeyChecker() = default;

std::future<KeyResult> APIKeyChecker::check_single_key_async(const std::string& api_key) {
    return std::async(std::launch::async, [this, api_key]() {
        return pImpl_->check_single_key(api_key);
    });
}

CheckResults APIKeyChecker::check_keys(const std::vector<std::string>& api_keys,
                                      size_t concurrent, bool quiet) {
    stats_.total = api_keys.size();
    stats_.start_time = std::chrono::system_clock::now();
    stats_.checked = 0;
    stats_.valid = 0;
    stats_.invalid = 0;
    stats_.error = 0;

    if (!quiet) {
        std::cout << "🚀 开始检测 " << api_keys.size() << " 个 API keys..." << std::endl;
        std::cout << "⚡ 并发数: " << concurrent << std::endl;
        std::cout << "⏱️  请求超时: " << stats_.timeout_used << " 秒" << std::endl;
        std::cout << "🌐 目标 API: https://api.openai.com/v1/models" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }

    // 创建进度条
    ProgressBar progress_bar(api_keys.size(), !quiet);

    CheckResults results;
    results.stats = stats_;

    // 使用线程池进行并发检测
    std::vector<std::future<KeyResult>> futures;
    std::mutex results_mutex;

    // 创建线程池控制并发数量
    std::atomic<size_t> active_threads{0};
    const size_t max_concurrent = concurrent;

    for (const auto& key : api_keys) {
        if (should_stop_.load()) {
            break;
        }

        futures.emplace_back(std::async(std::launch::async, [&, key]() {
            // 等待可用线程槽位
            while (active_threads.load() >= max_concurrent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            active_threads.fetch_add(1);
            auto result = pImpl_->check_single_key(key);
            active_threads.fetch_sub(1);

            // 更新统计
            stats_.checked.fetch_add(1);

            {
                std::lock_guard<std::mutex> lock(results_mutex);
                switch (result.status) {
                    case KeyStatus::Valid:
                        stats_.valid.fetch_add(1);
                        results.valid_keys.push_back(result);
                        break;
                    case KeyStatus::Invalid:
                        stats_.invalid.fetch_add(1);
                        results.invalid_keys.push_back(result);
                        break;
                    case KeyStatus::Error:
                        stats_.error.fetch_add(1);
                        results.error_keys.push_back(result);
                        break;
                }
            }

            // 更新进度条
            if (!quiet) {
                std::string message = "🟢" + std::to_string(stats_.valid.load()) +
                                    " | 🔴" + std::to_string(stats_.invalid.load()) +
                                    " | ⚠️" + std::to_string(stats_.error.load());
                progress_bar.set_message(message);
                progress_bar.update(stats_.checked.load());
            }

            return result;
        }));
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }

    if (!quiet) {
        progress_bar.finish("检测完成!");
    }

    stats_.end_time = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        stats_.end_time - stats_.start_time);
    stats_.duration_secs = duration.count() / 1000.0;
    stats_.avg_speed = stats_.total / stats_.duration_secs;

    results.stats = stats_;
    return results;
}

void APIKeyChecker::stop() {
    should_stop_.store(true);
}

} // namespace api_checker
// 带进度保存的批量检测
CheckResults APIKeyChecker::check_keys_with_progress(const std::vector<std::string>& api_keys,
                                                    const std::string& input_file,
                                                    size_t concurrent, bool quiet) {
    // 生成进度文件名
    current_progress_file_ = "progress_" + current_session_id_ + ".json";

    // 初始化进度
    CheckProgress progress;
    progress.session_id = current_session_id_;
    progress.input_file = input_file;
    progress.all_keys = api_keys;
    progress.concurrent_used = concurrent;
    progress.timeout_used = stats_.timeout_used;
    progress.stats = stats_;
    progress.stats.total = api_keys.size();
    progress.stats.start_time = std::chrono::system_clock::now();
    progress.last_save_time = std::chrono::system_clock::now();

    // 保存初始进度
    save_progress(progress, current_progress_file_);

    return check_keys_internal(api_keys, concurrent, quiet, &progress);
}

// 从进度文件恢复检测
CheckResults APIKeyChecker::resume_from_progress(const std::string& progress_file, bool quiet) {
    auto progress_opt = load_progress(progress_file);
    if (!progress_opt) {
        throw std::runtime_error("无法加载进度文件: " + progress_file);
    }

    CheckProgress progress = *progress_opt;
    current_progress_file_ = progress_file;
    current_session_id_ = progress.session_id;

    // 获取未处理的keys
    auto pending_keys = progress.get_pending_keys();

    if (!quiet) {
        std::cout << "🔄 恢复检测进度..." << std::endl;
        std::cout << "📁 原始文件: " << progress.input_file << std::endl;
        std::cout << "📊 总计: " << progress.all_keys.size() << " 个" << std::endl;
        std::cout << "✅ 已完成: " << progress.completed_results.size() << " 个" << std::endl;
        std::cout << "⏳ 待处理: " << pending_keys.size() << " 个" << std::endl;
        std::cout << "🕐 上次保存: " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(progress.last_save_time)), "%Y-%m-%d %H:%M:%S") << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }

    if (pending_keys.empty()) {
        if (!quiet) {
            std::cout << "✅ 所有API Keys已检测完成！" << std::endl;
        }

        // 构建最终结果
        CheckResults results;
        results.stats = progress.stats;

        for (const auto& result : progress.completed_results) {
            switch (result.status) {
                case KeyStatus::Valid:
                    results.valid_keys.push_back(result);
                    break;
                case KeyStatus::Invalid:
                    results.invalid_keys.push_back(result);
                    break;
                case KeyStatus::Error:
                    results.error_keys.push_back(result);
                    break;
                default:
                    break;
            }
        }

        return results;
    }

    // 恢复统计信息
    stats_ = progress.stats;

    return check_keys_internal(pending_keys, progress.concurrent_used, quiet, &progress);
}

// 内部检测方法（支持进度保存）
CheckResults APIKeyChecker::check_keys_internal(const std::vector<std::string>& api_keys,
                                               size_t concurrent, bool quiet,
                                               CheckProgress* progress) {
    if (api_keys.empty()) {
        CheckResults empty_results;
        empty_results.stats = stats_;
        return empty_results;
    }

    if (!progress) {
        // 如果没有进度对象，使用原始方法
        return check_keys(api_keys, concurrent, quiet);
    }

    if (!quiet) {
        std::cout << "🚀 开始检测 " << api_keys.size() << " 个 API keys..." << std::endl;
        std::cout << "⚡ 并发数: " << concurrent << std::endl;
        std::cout << "⏱️  请求超时: " << stats_.timeout_used << " 秒" << std::endl;
        std::cout << "🌐 目标 API: https://api.openai.com/v1/models" << std::endl;
        std::cout << "💾 进度文件: " << current_progress_file_ << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }

    // 创建进度条
    ProgressBar progress_bar(progress->all_keys.size(), !quiet);
    progress_bar.update(progress->completed_results.size());

    CheckResults results;
    results.stats = stats_;

    // 使用线程池进行并发检测
    std::vector<std::future<KeyResult>> futures;
    std::mutex results_mutex;

    // 创建线程池控制并发数量
    std::atomic<size_t> active_threads{0};
    const size_t max_concurrent = concurrent;

    for (const auto& key : api_keys) {
        if (should_stop_.load()) {
            break;
        }

        futures.emplace_back(std::async(std::launch::async, [&, key]() {
            // 等待可用线程槽位
            while (active_threads.load() >= max_concurrent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            active_threads.fetch_add(1);
            auto result = pImpl_->check_single_key(key);
            active_threads.fetch_sub(1);

            // 更新统计
            stats_.checked.fetch_add(1);

            {
                std::lock_guard<std::mutex> lock(results_mutex);

                // 添加到进度中
                progress->completed_results.push_back(result);
                progress->processed_keys.insert(key);

                switch (result.status) {
                    case KeyStatus::Valid:
                        stats_.valid.fetch_add(1);
                        results.valid_keys.push_back(result);
                        break;
                    case KeyStatus::Invalid:
                        stats_.invalid.fetch_add(1);
                        results.invalid_keys.push_back(result);
                        break;
                    case KeyStatus::Error:
                        stats_.error.fetch_add(1);
                        results.error_keys.push_back(result);
                        break;
                    default:
                        break;
                }

                // 定期保存进度
                auto now = std::chrono::steady_clock::now();
                if (now - last_save_time_ >= SAVE_INTERVAL) {
                    progress->stats = stats_;
                    progress->last_save_time = std::chrono::system_clock::now();
                    save_progress(*progress, current_progress_file_);
                    last_save_time_ = now;
                }
            }

            // 更新进度条
            if (!quiet) {
                std::string message = "🟢" + std::to_string(stats_.valid.load()) +
                                    " | 🔴" + std::to_string(stats_.invalid.load()) +
                                    " | ⚠️" + std::to_string(stats_.error.load());
                progress_bar.set_message(message);
                progress_bar.update(progress->completed_results.size());
            }

            return result;
        }));
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }

    if (!quiet) {
        progress_bar.finish("检测完成!");
    }

    stats_.end_time = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        stats_.end_time - stats_.start_time);
    stats_.duration_secs = duration.count() / 1000.0;
    stats_.avg_speed = stats_.total / stats_.duration_secs;

    // 最终保存进度
    progress->stats = stats_;
    progress->last_save_time = std::chrono::system_clock::now();
    save_progress(*progress, current_progress_file_);

    results.stats = stats_;
    return results;
}

// 保存当前进度
bool APIKeyChecker::save_progress(const CheckProgress& progress, const std::string& progress_file) {
    try {
        std::string filename = progress_file.empty() ? current_progress_file_ : progress_file;
        auto json_content = progress.to_json().dump(2);
        return FileUtils::write_file(filename, json_content);
    } catch (const std::exception& e) {
        std::cerr << "保存进度失败: " << e.what() << std::endl;
        return false;
    }
}

// 加载进度文件
std::optional<CheckProgress> APIKeyChecker::load_progress(const std::string& progress_file) {
    try {
        auto content = FileUtils::read_file(progress_file);
        if (!content) {
            return std::nullopt;
        }

        auto json_data = nlohmann::json::parse(*content);
        return CheckProgress::from_json(json_data);
    } catch (const std::exception& e) {
        std::cerr << "加载进度失败: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// 查找最新的进度文件
std::optional<std::string> APIKeyChecker::find_latest_progress_file(const std::string& input_file) {
    try {
        std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> progress_files;

        // 查找所有进度文件
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.is_regular_file()) {
                auto filename = entry.path().filename().string();
                if (filename.length() > 9 && filename.substr(0, 9) == "progress_" &&
                    filename.length() > 5 && filename.substr(filename.length() - 5) == ".json") {
                    // 尝试加载并检查是否匹配输入文件
                    auto progress_opt = load_progress(filename);
                    if (progress_opt && progress_opt->input_file == input_file) {
                        progress_files.emplace_back(filename, progress_opt->last_save_time);
                    }
                }
            }
        }

        if (progress_files.empty()) {
            return std::nullopt;
        }

        // 找到最新的进度文件
        auto latest = std::max_element(progress_files.begin(), progress_files.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });

        return latest->first;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}