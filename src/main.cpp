#include <iostream>
#include "absl/strings/str_join.h"  // 使用 Abseil 的字符串库

#include "mymodule.h" // 包含自定义模块

#include "base64.h"

#include "gtest/gtest.h"

TEST(MyTest, Test1) {
    ASSERT_EQ(1 + 1, 2);
}

int main() {
    std::string str1 = "Hello";
    std::string str2 = "Abseil";
    std::string str3 = "!";
    std::string str4 = "Tom";
    // 使用 absl::StrCat 进行字符串拼接
    std::string result = absl::StrCat(str1, ", ", str2, str3);
    std::cout << result << std::endl;
    mymodule_function(); // 调用自定义函数

    std::string encoded = base64Encode(str4);
    std::cout << str4 << " after encoded is : " << encoded << std::endl;
    std::cout << encoded << " after decoded is : " << base64Decode(encoded) << std::endl;
    std::string str5 = "Lucy";
    encoded = base64Encode(str5);
    std::cout << str5 << " after encoded is : " << encoded << std::endl;
    std::cout << encoded << " after decoded is : " << base64Decode(encoded) << std::endl;

    std::string file1 = "output.txt";
    std::string file2 = "test2.txt";
    // base64StreamEncode(file1, file2);
    base64StreamDecode(file1, file2);

    // googletest 
    ::testing::InitGoogleTest();
    RUN_ALL_TESTS();
    return 0;
}

