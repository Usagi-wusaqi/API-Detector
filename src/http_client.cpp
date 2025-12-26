#include "http_client.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>

namespace api_checker {

// 回调函数用于接收HTTP响应数据
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

class HttpClient::Impl {
public:
    Impl() {
        curl_ = curl_easy_init();
        if (!curl_) {
            throw std::runtime_error("Failed to initialize libcurl");
        }

        // 设置默认选项
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, "api-key-checker/1.0");
    }

    ~Impl() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }

    void set_timeout(std::chrono::seconds timeout) {
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout.count());
    }

    void set_connect_timeout(std::chrono::seconds timeout) {
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, timeout.count());
    }

    void set_user_agent(const std::string& user_agent) {
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, user_agent.c_str());
    }

    HttpResponse get(const std::string& url, const std::vector<std::string>& headers) {
        HttpResponse response;
        auto start_time = std::chrono::steady_clock::now();

        if (!curl_) {
            response.error_message = "CURL not initialized";
            return response;
        }

        std::string response_body;
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);

        // 设置请求头
        struct curl_slist* header_list = nullptr;
        for (const auto& header : headers) {
            header_list = curl_slist_append(header_list, header.c_str());
        }
        if (header_list) {
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
        }

        // 执行请求
        CURLcode res = curl_easy_perform(curl_);
        auto end_time = std::chrono::steady_clock::now();

        // 清理请求头
        if (header_list) {
            curl_slist_free_all(header_list);
        }

        response.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        if (res != CURLE_OK) {
            response.error_message = curl_easy_strerror(res);
            response.success = false;
            return response;
        }

        // 获取HTTP状态码
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status_code);
        response.body = std::move(response_body);
        response.success = true;

        return response;
    }

private:
    CURL* curl_;
};

HttpClient::HttpClient() : pImpl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

void HttpClient::set_timeout(std::chrono::seconds timeout) {
    pImpl_->set_timeout(timeout);
}

void HttpClient::set_connect_timeout(std::chrono::seconds timeout) {
    pImpl_->set_connect_timeout(timeout);
}

void HttpClient::set_user_agent(const std::string& user_agent) {
    pImpl_->set_user_agent(user_agent);
}

HttpResponse HttpClient::get(const std::string& url, const std::vector<std::string>& headers) {
    return pImpl_->get(url, headers);
}

} // namespace api_checker