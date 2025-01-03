#include "logger.h"

// 静态成员变量定义
std::shared_ptr<spdlog::logger> Logger::instance = nullptr;
std::once_flag Logger::initFlag;