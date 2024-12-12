#include "gtest/gtest.h"

// 待测试的类
class  Calculator {
public:
    int Add(int a, int b) { return a + b; }
    int Subtract(int a, int b) { return a - b; }
};

// 测试夹具类
class CalculatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        calculator_ = new Calculator();
    }

    void TearDown() override {
        delete calculator_;
    }

    Calculator* calculator_;
};

// 测试用例
TEST_F(CalculatorTest, AddTest) {
    ASSERT_EQ(calculator_->Add(2, 3), 5);
}

TEST_F(CalculatorTest, SubtractTest) {
    ASSERT_EQ(calculator_->Subtract(5,2), 3);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}