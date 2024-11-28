#include <iostream>
#include <string_view>
#include "streamr-json/toJson.hpp"
#include "streamr-json/toString.hpp"

using streamr::json::StreamrJsonInitializerList;
using streamr::json::toJson;
using streamr::json::toString;

// A struct with public members that can be serialized directly

struct MyStruct {
    int x;
    std::string y;
};

// A template function that forwards a parameter to toJson()
// The default type parameter is important here
// Because C++ templating system cannot automatically
// infer initializer list types.

template <typename T = StreamrJsonInitializerList>
void printMyData(T data) { // NOLINT
    std::cout << toJson(data) << std::endl;
}

// A class with a private section.
// This class cannot be serialized directly
// Because of boost::pfr limitations.

class ClassWithPrivateSection {
private:
    int data;
    std::string name;

public:
    ClassWithPrivateSection(int data, std::string_view name)
        : data(data), name(name) {}

    // We need to provide a toJson()/toString() methods to be able
    // to serialize the class because it has private sections

    [[nodiscard]] nlohmann::json toJson() const {
        return {{"data", data}, {"name", name}};
    }
    [[nodiscard]] std::string toString() const { return (toJson()).dump(); }
};

int main() {
    MyStruct s{.x = 1, .y = "hello"};

    // Converting a struct to a json object
    nlohmann::json json = toJson(s);

    // outputs: {"x":1,"y":"hello"}
    std::cout << json << '\n';

    // Converting an initializer list containing a struct to a json object

    nlohmann::json json2 = toJson({{"data", s}});

    // outputs: {"data":{"x":1,"y":"hello"}}
    std::cout << json2 << '\n';

    // Converting struct to a string
    std::string str = toString(s);

    // outputs: {"x":1,"y":"hello"}
    std::cout << str << '\n';

    // Calling a template function that forwards parameters to toJson()
    // Only works if the template function has the right default type parameter
    // outputs {"forwardedData":{"x":1,"y":"hello"}}
    printMyData({{"forwardedData", s}});

    // Converting a class with a private section to a json object
    ClassWithPrivateSection c{1, "hello"};
    nlohmann::json json3 = toJson(c);
    // outputs: {"data":1,"name":"hello"}
    std::cout << json3 << '\n';

    // Converting a class with a private section to a string
    std::string str2 = toString(c);
    // outputs: {"data":1,"name":"hello"}
    std::cout << str2 << '\n';

    return 0;
}