#include <iostream>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "gtest/gtest.h"

TEST(MyTest, Test1) {
    ASSERT_EQ(1 + 1, 2);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "Boost is working!" << std::endl;
    boost::system::error_code ec;
    boost::thread t([](){});
    t.join();
    boost::filesystem::path p("/tmp");
    std::cout << p.string() << std::endl;
    return RUN_ALL_TESTS();
}