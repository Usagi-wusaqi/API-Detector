#include "file_utils.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <regex>
#include <chrono>
#include <iomanip>

namespace api_checker {

std::optional<std::string> FileUtils::read_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool FileUtils::write_file(const std::string& file_path, const std::string& content) {
    try {
        // 确保目录存在
        auto parent_path = std::filesystem::path(file_path).parent_path();
        if (!parent_path.empty()) {
            std::filesystem::create_directories(parent_path);
        }

        std::ofstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        file << content;
        return file.good();
    } catch (const std::exception&) {
        return false;
    }
}

bool FileUtils::file_exists(const std::string& file_path) {
    return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
}

bool FileUtils::create_directory(const std::string& dir_path) {
    try {
        return std::filesystem::create_directories(dir_path);
    } catch (const std::exception&) {
        return false;
    }
}

std::optional<size_t> FileUtils::get_file_size(const std::string& file_path) {
    try {
        if (!file_exists(file_path)) {
            return std::nullopt;
        }
        return std::filesystem::file_size(file_path);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<std::string> FileUtils::load_api_keys(const std::string& file_path) {
    std::vector<std::string> keys;

    auto content = read_file(file_path);
    if (!content) {
        return keys;
    }

    std::istringstream stream(*content);
    std::string line;
    std::regex sk_pattern(R"(sk-[a-zA-Z0-9]{48,})");

    while (std::getline(stream, line)) {
        // 去除首尾空白字符
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') {
            continue; // 跳过空行和注释行
        }

        // 检查是否是有效的API Key格式
        if (std::regex_search(line, sk_pattern)) {
            // 提取sk-开头的部分
            std::smatch match;
            if (std::regex_search(line, match, sk_pattern)) {
                keys.push_back(match.str());
            }
        }
    }

    return keys;
}

std::optional<std::string> FileUtils::find_api_keys_file() {
    // 按优先级查找API Keys文件
    std::vector<std::string> candidates = {
        "5w-sk.txt",
        "api_keys.txt",
        "keys.txt",
        "openai_keys.txt",
        "sk.txt"
    };

    for (const auto& filename : candidates) {
        if (file_exists(filename)) {
            return filename;
        }
    }

    // 在当前目录查找任何包含sk-的txt文件
    try {
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                if (path.extension() == ".txt") {
                    auto content = read_file(path.string());
                    if (content && content->find("sk-") != std::string::npos) {
                        return path.string();
                    }
                }
            }
        }
    } catch (const std::exception&) {
        // 忽略目录访问错误
    }

    return std::nullopt;
}

std::string FileUtils::generate_timestamp_filename(const std::string& prefix,
                                                  const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream ss;
    if (!prefix.empty()) {
        ss << prefix << "_";
    }
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    if (!extension.empty()) {
        ss << "." << extension;
    }

    return ss.str();
}

} // namespace api_checker