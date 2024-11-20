#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <gtest/gtest.h>

#include "streamr-json/toString.hpp"

#include "TestClass.hpp"
#include "WeatherData.hpp"

using streamr::json::toString;

// NOLINTBEGIN(readability-magic-numbers)

class ToStringTest : public ::testing::Test {
protected:
    WeatherData weatherData; // NOLINT
    TestClass testClass; // NOLINT

    void SetUp() override {
        weatherData = {
            .dataId = 0,
            .dataLabel = "Test data",
            .dataByCountry = {
                {"Finland",
                 {
                     {.locality = "Helsinki",
                      .temperatures =
                          {
                              {.temperature = 23.4,
                               .timestamp = std::time(nullptr)}, // NOLINT
                              {.temperature = 24.5,
                               .timestamp = std::time(nullptr) + 1000} // NOLINT
                          }},
                 }},
                {"Sweden",
                 {{.locality = "Stockholm",
                   .temperatures = {
                       {.temperature = 22.0,
                        .timestamp = std::time(nullptr)}, // NOLINT
                       {.temperature = 21.6,
                        .timestamp = std::time(nullptr) + 1000} // NOLINT
                   }}}}}};
    }
};
/*
TEST_F(ToStringTest, TestInitializerListToString) {
    EXPECT_EQ(
        toString(
            {{"pi", 3.141},
             {"happy", true},
             {"name", "Niels"},
             {"nothing", nullptr},
             {"answer", {{"everything", 42}}},
             {"list", {1, 0, 2}},
             {"object", {{"currency", "USD"}, {"value", 42.99}}}}),
        "{\"answer\":{\"everything\":42},\"happy\":true,\"list\":[1,0,2],\"name\":\"Niels\",\"nothing\":null,\"object\":{\"currency\":\"USD\",\"value\":42.99},\"pi\":3.141}");
}
*/
TEST_F(ToStringTest, TestWeatherDataToString) {
    // toString(weatherData);
}

TEST_F(ToStringTest, TestIntToString) {
    int a = 10; // NOLINT
    EXPECT_EQ(toString(a), "10");
}

TEST_F(ToStringTest, TestDoubleToString) {
    double b = 20.5; // NOLINT
    EXPECT_EQ(toString(b), "20.5");
}

TEST_F(ToStringTest, TestCharToString) {
    char c = 'c';
    EXPECT_EQ(
        toString(c), "99"); // char is a number type in C++, 'c' is 99 in ASCII
}

TEST_F(ToStringTest, TestStringToString) {
    std::string d = "Hello, World!";
    EXPECT_EQ(toString(d), "\"Hello, World!\"");
}

TEST_F(ToStringTest, TestBoolToString) {
    bool e = true;
    EXPECT_EQ(toString(e), "true");
}

TEST_F(ToStringTest, TestVectorToString) {
    std::vector<int> f = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toString(f), "[1,2,3,4,5]");
}

TEST_F(ToStringTest, TestMapToString) {
    std::map<std::string, int> g = {{"one", 1}, {"two", 2}, {"three", 3}};
    // std::map orders keys alphabetically
    EXPECT_EQ(toString(g), "{\"one\":1,\"three\":3,\"two\":2}");
}

TEST_F(ToStringTest, TestPairToString) {
    std::pair<int, std::string> h = {1, "one"};
    EXPECT_EQ(toString(h), "[1,\"one\"]");
}

TEST_F(ToStringTest, TestSetToString) {
    std::set<int> i = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toString(i), "[1,2,3,4,5]");
}

TEST_F(ToStringTest, TestArrayToString) {
    std::array<int, 5> j = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toString(j), "[1,2,3,4,5]");
}

TEST_F(ToStringTest, TestFloatToString) {
    float k = 30.5f; // NOLINT
    EXPECT_EQ(toString(k), "30.5");
}

TEST_F(ToStringTest, TestLongToString) {
    long l = 1000000L; // NOLINT
    EXPECT_EQ(toString(l), "1000000");
}

TEST_F(ToStringTest, TestShortToString) {
    short m = 10; // NOLINT
    EXPECT_EQ(toString(m), "10");
}

TEST_F(ToStringTest, TestUnsignedIntToString) {
    unsigned int n = 20; // NOLINT
    EXPECT_EQ(toString(n), "20");
}

TEST_F(ToStringTest, TestLongDoubleToString) {
    long double o = 30.5L; // NOLINT
    EXPECT_EQ(toString(o), "30.5");
}

TEST_F(ToStringTest, TestUnsignedLongToString) {
    unsigned long p = 1000000UL; // NOLINT
    EXPECT_EQ(toString(p), "1000000");
}

TEST_F(ToStringTest, TestUnsignedShortToString) {
    unsigned short q = 10; // NOLINT
    EXPECT_EQ(toString(q), "10");
}

TEST_F(ToStringTest, TestListToString) {
    std::list<int> r = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toString(r), "[1,2,3,4,5]");
}

TEST_F(ToStringTest, TestDequeToString) {
    std::deque<int> s = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toString(s), "[1,2,3,4,5]");
}

TEST_F(ToStringTest, TestForwardListToString) {
    std::forward_list<int> t = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toString(t), "[1,2,3,4,5]");
}

TEST_F(ToStringTest, TestClassToString) {
    // std::string expectedString = "Hello world!";
    // EXPECT_EQ(toString(testClass), expectedString);
}

// NOLINTEND(readability-magic-numbers)
