#ifndef STREAMR_JSON_TEST_UNIT_TESTCLASS_HPP
#define STREAMR_JSON_TEST_UNIT_TESTCLASS_HPP

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class TestClass {
private:
    int a;
    std::string b;

public:
    [[nodiscard]] int getA() const { return a; }
    [[nodiscard]] std::string getB() const { return b; }
    [[nodiscard]] std::string toString() const {
        return std::string("Hello world!");
    } // NOLINT
    [[nodiscard]] json toJson() const { // NOLINT
        return json{{"a", a}, {"b", b}};
    }
};

#endif
