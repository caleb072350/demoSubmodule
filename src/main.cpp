#include <iostream>
#include "absl/strings/str_join.h"  // 使用 Abseil 的字符串库

#include "mymodule/mymodule.cpp" // 包含自定义模块

int main() {
    std::cout << "Hello from demo!" << std::endl;
    mymodule_function(); // 调用自定义函数
    return 0;
}

