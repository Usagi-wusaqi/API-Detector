#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace api_checker {

struct AppConfig {
    // 检测设置
    size_t default_concurrent = 1000;
    size_t default_timeout = 10;
    size_t default_connect_timeout = 5;
    std::string default_output_dir = ".";

    // 进度设置
    bool auto_save_progress = true;
    size_t save_interval_seconds = 30;
    bool auto_resume_on_startup = false;
    size_t max_progress_files = 10;  // 最多保留的进度文件数量

    // 界面设置
    bool show_progress_bar = true;
    bool colored_output = true;
    std::string log_level = "info";  // debug, info, warn, error

    // API设置
    std::vector<std::string> supported_providers = {"openai"};
    std::string openai_api_base = "https://api.openai.com/v1";
    std::string openai_test_endpoint = "/models";

    // 文件设置
    std::vector<std::string> auto_detect_files = {
        "5w-sk.txt", "api_keys.txt", "keys.txt", "openai_keys.txt", "sk.txt"
    };

    nlohmann::json to_json() const;
    static AppConfig from_json(const nlohmann::json& j);
};

class ConfigManager {
public:
    ConfigManager();

    // 加载配置
    bool load_config(const std::string& config_file = "");

    // 保存配置
    bool save_config(const std::string& config_file = "") const;

    // 获取配置
    const AppConfig& get_config() const { return config_; }
    AppConfig& get_config() { return config_; }

    // 获取默认配置文件路径
    static std::string get_default_config_path();

    // 创建默认配置文件
    bool create_default_config();

private:
    AppConfig config_;
    std::string config_file_path_;
};

} // namespace api_checker