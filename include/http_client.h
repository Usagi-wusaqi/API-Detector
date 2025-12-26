#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <vector>

namespace api_checker {

struct HttpResponse {
    long status_code = 0;
    std::string body;
    std::chrono::milliseconds response_time{0};
    bool success = false;
    std::string error_message;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // 设置超时时间
    void set_timeout(std::chrono::seconds timeout);
    void set_connect_timeout(std::chrono::seconds timeout);

    // 发送GET请求
    HttpResponse get(const std::string& url,
                    const std::vector<std::string>& headers = {});

    // 设置User-Agent
    void set_user_agent(const std::string& user_agent);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace api_checker