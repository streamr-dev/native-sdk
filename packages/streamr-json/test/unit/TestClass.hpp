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
    // NOLINTNEXTLINE (readability-convert-member-functions-to-static)
    [[nodiscard]] std::string toString() const {
        return std::string{"Hello world!"};
    }
    // NOLINTNEXTLINE (readability-convert-member-functions-to-static)
    [[nodiscard]] json toJson() const { return json{{"a", a}, {"b", b}}; }
};

#endif
