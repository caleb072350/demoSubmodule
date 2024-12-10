#include <iostream>
#include "absl/strings/str_join.h"  // 使用 Abseil 的字符串库

#include "mymodule/mymodule.cpp" // 包含自定义模块

int main() {
    std::string str1 = "Hello";
    std::string str2 = "Abseil";
    std::string str3 = "!";
    // 使用 absl::StrCat 进行字符串拼接
    std::string result = absl::StrCat(str1, ", ", str2, str3);
    std::cout << result << std::endl;
    mymodule_function(); // 调用自定义函数
    return 0;
}

