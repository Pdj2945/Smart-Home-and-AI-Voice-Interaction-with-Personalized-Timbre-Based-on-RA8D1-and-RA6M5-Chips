#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <thread>      // 添加线程支持
#include <mutex>       // 添加互斥锁支持
#include <condition_variable>  // 添加条件变量支持
#include <queue>
#include <chrono>
#include <mosquitto.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 配置信息
constexpr const char* MQTT_BROKER = "bemfa.com";
constexpr int MQTT_PORT = 9504;
constexpr const char* MQTT_TOPIC = "text1";
constexpr const char* MQTT_CLIENT_ID = "your_client_id";  // 替换为你的客户端ID
constexpr const char* MQTT_USERNAME = "your_username";    // 替换为你的用户名
constexpr const char* MQTT_PASSWORD = "your_password";    // 替换为你的密码
constexpr const char* DEEPSEEK_API_KEY = "your_api_key";  // 替换为你的DeepSeek API密钥
constexpr const char* DEEPSEEK_API_URL = "https://api.deepseek.com/v1/chat/completions";

class CloudBridge {
private:
    // MQTT客户端
    std::unique_ptr<mosquitto, std::function<void(mosquitto*)>> mosq;
    
    // 消息队列
    std::queue<std::string> message_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    
    // 工作线程
    std::thread worker_thread;
    bool running = false;

    // CURL全局初始化
    static class CurlGlobalInit {
    public:
        CurlGlobalInit() { curl_global_init(CURL_GLOBAL_ALL); }
        ~CurlGlobalInit() { curl_global_cleanup(); }
    } curl_init;

    // 回调函数
    static void on_connect(struct mosquitto* mosq, void* obj, int rc) {
        auto* bridge = static_cast<CloudBridge*>(obj);
        if (rc == 0) {
            std::cout << "成功连接到巴法云MQTT服务器" << std::endl;
            mosquitto_subscribe(mosq, nullptr, MQTT_TOPIC, 0);
        } else {
            std::cerr << "连接失败，返回码: " << rc << std::endl;
        }
    }

    static void on_message(struct mosquitto* mosq, void* obj, 
                          const struct mosquitto_message* msg) {
        auto* bridge = static_cast<CloudBridge*>(obj);
        std::string message(static_cast<char*>(msg->payload), msg->payloadlen);
        bridge->handle_message(message);
    }

    // 处理接收到的消息
    void handle_message(const std::string& message) {
        std::cout << "收到消息: " << message << std::endl;
        
        // 调用DeepSeek API
        std::string response = call_deepseek_api(message);
        
        // 将响应加入队列
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            message_queue.push(response);
        }
        queue_cv.notify_one();
    }

    // 调用DeepSeek API
    std::string call_deepseek_api(const std::string& message) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return "初始化CURL失败";
        }

        // 准备JSON数据
        json request_data = {
            {"model", "deepseek-chat"},
            {"messages", {
                {{"role", "user"}, {"content", message}}
            }},
            {"temperature", 0.7}
        };

        std::string json_str = request_data.dump();
        std::string response_data;

        // 设置CURL选项
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string auth_header = "Authorization: Bearer " + std::string(DEEPSEEK_API_KEY);
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, DEEPSEEK_API_URL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
            [](void* ptr, size_t size, size_t nmemb, std::string* data) {
                data->append(static_cast<char*>(ptr), size * nmemb);
                return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        // 执行请求
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return "调用DeepSeek API失败: " + std::string(curl_easy_strerror(res));
        }

        try {
            json response_json = json::parse(response_data);
            return response_json["choices"][0]["message"]["content"].get<std::string>();
        } catch (const std::exception& e) {
            return "解析响应失败: " + std::string(e.what());
        }
    }

    // 工作线程函数
    void worker_function() {
        while (running) {
            std::string message;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this] { 
                    return !message_queue.empty() || !running; 
                });
                
                if (!running && message_queue.empty()) {
                    break;
                }
                
                message = message_queue.front();
                message_queue.pop();
            }

            // 发布消息到巴法云
            mosquitto_publish(mosq.get(), nullptr, MQTT_TOPIC, 
                            message.length(), message.c_str(), 0, false);
            std::cout << "已发送消息到巴法云: " << message << std::endl;
        }
    }

public:
    CloudBridge() {
        // 初始化MQTT客户端
        mosquitto_lib_init();
        mosq = std::unique_ptr<mosquitto, std::function<void(mosquitto*)>>(
            mosquitto_new(MQTT_CLIENT_ID, true, this),
            [](mosquitto* m) { mosquitto_destroy(m); }
        );

        if (!mosq) {
            throw std::runtime_error("创建MQTT客户端失败");
        }

        // 设置回调函数
        mosquitto_connect_callback_set(mosq.get(), on_connect);
        mosquitto_message_callback_set(mosq.get(), on_message);
        mosquitto_username_pw_set(mosq.get(), MQTT_USERNAME, MQTT_PASSWORD);
    }

    ~CloudBridge() {
        stop();
        mosquitto_lib_cleanup();
    }

    void start() {
        running = true;
        
        // 连接MQTT服务器
        if (mosquitto_connect(mosq.get(), MQTT_BROKER, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
            throw std::runtime_error("连接MQTT服务器失败");
        }

        // 启动工作线程
        worker_thread = std::thread(&CloudBridge::worker_function, this);

        // 启动MQTT循环
        mosquitto_loop_forever(mosq.get(), -1, 1);
    }

    void stop() {
        running = false;
        queue_cv.notify_all();
        
        if (worker_thread.joinable()) {
            worker_thread.join();
        }

        if (mosq) {
            mosquitto_disconnect(mosq.get());
        }
    }
};

int main() {
    try {
        CloudBridge bridge;
        std::cout << "启动云端桥接服务..." << std::endl;
        bridge.start();
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}