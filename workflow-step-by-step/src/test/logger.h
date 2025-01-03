#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <mutex>

class Logger {
public:
    // 获取 Logger 实例
    static std::shared_ptr<spdlog::logger> getInstance() {
        std::call_once(initFlag, []() {
            instance = spdlog::basic_logger_mt("file_logger", "logs/spdlog.log");
            instance->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
            instance->set_level(spdlog::level::debug);
        });
        return instance;
    }

#define LOG_INFO(message, ...) \
       SPDLOG_LOGGER_INFO(Logger::getInstance(), message, ##__VA_ARGS__);

#define LOG_DEBUG(message, ...) \
       SPDLOG_LOGGER_DEBUG(Logger::getInstance(), message, ##__VA_ARGS__);

#define LOG_ERROR(message, ...) \
       SPDLOG_LOGGER_ERROR(Logger::getInstance(), message, ##__VA_ARGS__);
    
private:
    Logger() = default; // 私有化构造函数
    ~Logger() = default; // 私有化析构函数

    static std::shared_ptr<spdlog::logger> instance;
    static std::once_flag initFlag;
};

// 静态成员变量定义
std::shared_ptr<spdlog::logger> Logger::instance = nullptr;
std::once_flag Logger::initFlag;
