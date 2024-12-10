#include <iostream>
#include "absl/strings/str_join.h" // 使用 Abseil 的字符串库

void mymodule_function() {
    std::string str1 = "Hello";
    std::string str2 = "from";
    std::string str3 = "mymodule!";

    std::string result = absl::StrCat(str1, " ", str2, " ", str3);
    std::cout << result << std::endl;
}