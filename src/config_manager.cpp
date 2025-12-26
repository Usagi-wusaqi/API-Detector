#include "config_manager.h"
#include "file_utils.h"
#include <filesystem>
#include <iostream>

namespace api_checker {

// AppConfig JSON序列化
nlohmann::json AppConfig::to_json() const {
    nlohmann::json j;

    // 检测设置
    j["detection"] = {
        {"default_concurrent", default_concurrent},
        {"default_timeout", default_timeout},
        {"default_connect_timeout", default_connect_timeout},
        {"default_output_dir", default_output_dir}
    };

    // 进度设置
    j["progress"] = {
        {"auto_save_progress", auto_save_progress},
        {"save_interval_seconds", save_interval_seconds},
        {"auto_resume_on_startup", auto_resume_on_startup},
        {"max_progress_files", max_progress_files}
    };

    // 界面设置
    j["ui"] = {
        {"show_progress_bar", show_progress_bar},
        {"colored_output", colored_output},
        {"log_level", log_level}
    };

    // API设置
    j["api"] = {
        {"supported_providers", supported_providers},
        {"openai_api_base", openai_api_base},
        {"openai_test_endpoint", openai_test_endpoint}
    };

    // 文件设置
    j["files"] = {
        {"auto_detect_files", auto_detect_files}
    };

    return j;
}

// AppConfig JSON反序列化
AppConfig AppConfig::from_json(const nlohmann::json& j) {
    AppConfig config;

    if (j.contains("detection")) {
        const auto& detection = j["detection"];
        if (detection.contains("default_concurrent")) {
            config.default_concurrent = detection["default_concurrent"];
        }
        if (detection.contains("default_timeout")) {
            config.default_timeout = detection["default_timeout"];
        }
        if (detection.contains("default_connect_timeout")) {
            config.default_connect_timeout = detection["default_connect_timeout"];
        }
        if (detection.contains("default_output_dir")) {
            config.default_output_dir = detection["default_output_dir"];
        }
    }

    if (j.contains("progress")) {
        const auto& progress = j["progress"];
        if (progress.contains("auto_save_progress")) {
            config.auto_save_progress = progress["auto_save_progress"];
        }
        if (progress.contains("save_interval_seconds")) {
            config.save_interval_seconds = progress["save_interval_seconds"];
        }
        if (progress.contains("auto_resume_on_startup")) {
            config.auto_resume_on_startup = progress["auto_resume_on_startup"];
        }
        if (progress.contains("max_progress_files")) {
            config.max_progress_files = progress["max_progress_files"];
        }
    }

    if (j.contains("ui")) {
        const auto& ui = j["ui"];
        if (ui.contains("show_progress_bar")) {
            config.show_progress_bar = ui["show_progress_bar"];
        }
        if (ui.contains("colored_output")) {
            config.colored_output = ui["colored_output"];
        }
        if (ui.contains("log_level")) {
            config.log_level = ui["log_level"];
        }
    }

    if (j.contains("api")) {
        const auto& api = j["api"];
        if (api.contains("supported_providers")) {
            config.supported_providers = api["supported_providers"];
        }
        if (api.contains("openai_api_base")) {
            config.openai_api_base = api["openai_api_base"];
        }
        if (api.contains("openai_test_endpoint")) {
            config.openai_test_endpoint = api["openai_test_endpoint"];
        }
    }

    if (j.contains("files")) {
        const auto& files = j["files"];
        if (files.contains("auto_detect_files")) {
            config.auto_detect_files = files["auto_detect_files"];
        }
    }

    return config;
}

ConfigManager::ConfigManager() {
    config_file_path_ = get_default_config_path();
}

bool ConfigManager::load_config(const std::string& config_file) {
    std::string file_path = config_file.empty() ? config_file_path_ : config_file;

    if (!FileUtils::file_exists(file_path)) {
        // 如果配置文件不存在，创建默认配置
        return create_default_config();
    }

    try {
        auto content = FileUtils::read_file(file_path);
        if (!content) {
            std::cerr << "无法读取配置文件: " << file_path << std::endl;
            return false;
        }

        auto json_data = nlohmann::json::parse(*content);
        config_ = AppConfig::from_json(json_data);

        if (!config_file.empty()) {
            config_file_path_ = config_file;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "解析配置文件失败: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::save_config(const std::string& config_file) const {
    std::string file_path = config_file.empty() ? config_file_path_ : config_file;

    try {
        auto json_content = config_.to_json().dump(2);
        return FileUtils::write_file(file_path, json_content);
    } catch (const std::exception& e) {
        std::cerr << "保存配置文件失败: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::get_default_config_path() {
    return "api_checker_config.json";
}

bool ConfigManager::create_default_config() {
    try {
        // 使用默认配置
        config_ = AppConfig{};

        // 保存默认配置到文件
        auto json_content = config_.to_json();

        // 添加注释说明
        json_content["_comment"] = {
            "这是API检测器的配置文件",
            "修改后重启程序生效",
            "detection: 检测相关设置",
            "progress: 进度保存设置",
            "ui: 界面显示设置",
            "api: API相关设置",
            "files: 文件处理设置"
        };

        auto content = json_content.dump(2);
        return FileUtils::write_file(config_file_path_, content);
    } catch (const std::exception& e) {
        std::cerr << "创建默认配置失败: " << e.what() << std::endl;
        return false;
    }
}

} // namespace api_checker