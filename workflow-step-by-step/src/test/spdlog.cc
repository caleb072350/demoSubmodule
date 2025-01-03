#include "logger.h"
#include <iostream>

int main() {
    try {
            // auto logger = Logger::getInstance();

            // SPDLOG_LOGGER_INFO(logger, "这是一个文件日志示例");
            // SPDLOG_LOGGER_WARN(logger, "警告信息");
            // SPDLOG_LOGGER_ERROR(logger, "错误信息");

            LOG_ERROR("TEST, {}", "ERROR");

            // 刷新日志缓冲区
            // logger->flush();
    } catch (const spdlog::spdlog_ex& ex) {
        std::cout << "日志初始化失败: " << ex.what() << std::endl;
    }
}
