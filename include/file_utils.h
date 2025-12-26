#pragma once

#include <string>
#include <vector>
#include <optional>

namespace api_checker {

class FileUtils {
public:
    // 读取文件内容
    static std::optional<std::string> read_file(const std::string& file_path);

    // 写入文件内容
    static bool write_file(const std::string& file_path, const std::string& content);

    // 检查文件是否存在
    static bool file_exists(const std::string& file_path);

    // 创建目录
    static bool create_directory(const std::string& dir_path);

    // 获取文件大小
    static std::optional<size_t> get_file_size(const std::string& file_path);

    // 从文件加载API Keys
    static std::vector<std::string> load_api_keys(const std::string& file_path);

    // 查找API Keys文件
    static std::optional<std::string> find_api_keys_file();

    // 生成时间戳文件名
    static std::string generate_timestamp_filename(const std::string& prefix,
                                                  const std::string& extension);
};

} // namespace api_checker