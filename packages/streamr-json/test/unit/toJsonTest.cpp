#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <gtest/gtest.h>

#include "streamr-json/toJson.hpp"

#include "TestClass.hpp"
#include "WeatherData.hpp"

using streamr::json::toJson;

// NOLINTBEGIN(readability-magic-numbers)

class ToJsonTest : public ::testing::Test {
protected:
    WeatherData weatherData; // NOLINT
    TestClass testClass; // NOLINT

    void SetUp() override {
        weatherData = {
            .dataId = 0,
            .dataLabel = "Test data",
            .dataByCountry = {
                {
                    "Finland",
                    {.locality = "Helsinki",
                     .temperatures =
                         {
                             {.temperature = 23.4,
                              .timestamp = std::time(nullptr)}, // NOLINT
                             {.temperature = 24.5,
                              .timestamp = std::time(nullptr) + 1000} // NOLINT
                         }},
                },
                {"Sweden",
                 {.locality = "Stockholm",
                  .temperatures = {
                      {.temperature = 22.0,
                       .timestamp = std::time(nullptr)}, // NOLINT
                      {.temperature = 21.6,
                       .timestamp = std::time(nullptr) + 1000} // NOLINT
                  }}}}};
    }
};

TEST_F(ToJsonTest, TestWeatherDataToJson) {
    json expectedJson = {
        {"dataId", 0},
        {"dataLabel", "Test data"},
        {"dataByCountry",
         {{"Finland",
           {{"locality", "Helsinki"},
            {"temperatures",
             {{{"temperature", 23.4}, {"timestamp", std::time(nullptr)}},
              {{"temperature", 24.5},
               {"timestamp", std::time(nullptr) + 1000}}}}}},
          {"Sweden",
           {{"locality", "Stockholm"},
            {"temperatures",
             {{{"temperature", 22.0}, {"timestamp", std::time(nullptr)}},
              {{"temperature", 21.6},
               {"timestamp", std::time(nullptr) + 1000}}}}}}}}};
    EXPECT_EQ(toJson(weatherData), expectedJson);
}

TEST_F(ToJsonTest, TestInitializerListToJson) {
    EXPECT_EQ(
        toJson(
            {{"pi", 3.141},
             {"happy", true},
             {"name", "Niels"},
             {"nothing", nullptr},
             {"answer", {{"everything", 42}}},
             {"list", {1, 0, 2}},
             {"object", {{"currency", "USD"}, {"value", 42.99}}}}),
        json(
            {{"pi", 3.141},
             {"happy", true},
             {"name", "Niels"},
             {"nothing", nullptr},
             {"answer", {{"everything", 42}}},
             {"list", {1, 0, 2}},
             {"object", {{"currency", "USD"}, {"value", 42.99}}}}));
}

TEST_F(ToJsonTest, TestIntToJson) {
    int a = 10;
    EXPECT_EQ(toJson(a), 10);
}

TEST_F(ToJsonTest, TestDoubleToJson) {
    double b = 20.5;
    EXPECT_EQ(toJson(b), 20.5);
}

TEST_F(ToJsonTest, TestCharToJson) {
    char c = 'c';
    EXPECT_EQ(
        toJson(c), 99); // char is a number type in C++, 'c' is 99 in ASCII
}

TEST_F(ToJsonTest, TestStringToJson) {
    std::string d = "Hello, World!";
    EXPECT_EQ(toJson(d), "Hello, World!");
}

TEST_F(ToJsonTest, TestBoolToJson) {
    bool e = true;
    EXPECT_EQ(toJson(e), true);
}

TEST_F(ToJsonTest, TestVectorToJson) {
    std::vector<int> f = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toJson(f), json({1, 2, 3, 4, 5}));
}

TEST_F(ToJsonTest, TestMapToJson) {
    std::map<std::string, int> g = {{"one", 1}, {"two", 2}, {"three", 3}};
    // std::map orders keys alphabetically
    EXPECT_EQ(toJson(g), json({{"one", 1}, {"three", 3}, {"two", 2}}));
}

TEST_F(ToJsonTest, TestPairToJson) {
    std::pair<int, std::string> h = {1, "one"};
    EXPECT_EQ(toJson(h), json({1, "one"}));
}

TEST_F(ToJsonTest, TestSetToJson) {
    std::set<int> i = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toJson(i), json({1, 2, 3, 4, 5}));
}

TEST_F(ToJsonTest, TestArrayToJson) {
    std::array<int, 5> j = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toJson(j), json({1, 2, 3, 4, 5}));
}

TEST_F(ToJsonTest, TestFloatToJson) {
    float k = 30.5f; // NOLINT
    EXPECT_EQ(toJson(k), 30.5);
}

TEST_F(ToJsonTest, TestLongToJson) {
    long l = 1000000L; // NOLINT
    EXPECT_EQ(toJson(l), 1000000);
}

TEST_F(ToJsonTest, TestShortToJson) {
    short m = 10; // NOLINT
    EXPECT_EQ(toJson(m), 10);
}

TEST_F(ToJsonTest, TestUnsignedIntToJson) {
    unsigned int n = 20; // NOLINT
    EXPECT_EQ(toJson(n), 20);
}

TEST_F(ToJsonTest, TestLongDoubleToJson) {
    long double o = 30.5L; // NOLINT
    EXPECT_EQ(toJson(o), 30.5);
}

TEST_F(ToJsonTest, TestUnsignedLongToJson) {
    unsigned long p = 1000000UL; // NOLINT
    EXPECT_EQ(toJson(p), 1000000);
}

TEST_F(ToJsonTest, TestUnsignedShortToJson) {
    unsigned short q = 10; // NOLINT
    EXPECT_EQ(toJson(q), 10);
}

TEST_F(ToJsonTest, TestListToJson) {
    std::list<int> r = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toJson(r), json({1, 2, 3, 4, 5}));
}

TEST_F(ToJsonTest, TestDequeToJson) {
    std::deque<int> s = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toJson(s), json({1, 2, 3, 4, 5}));
}

TEST_F(ToJsonTest, TestForwardListToJson) {
    std::forward_list<int> t = {1, 2, 3, 4, 5}; // NOLINT
    EXPECT_EQ(toJson(t), json({1, 2, 3, 4, 5}));
}

TEST_F(ToJsonTest, TestClassToJson) {
    json expectedJson = {{"a", testClass.getA()}, {"b", testClass.getB()}};
    EXPECT_EQ(toJson(testClass), expectedJson);
}

// NOLINTEND(readability-magic-numbers)
