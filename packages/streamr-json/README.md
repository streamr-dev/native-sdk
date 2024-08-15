# Streamr-json

`streamr-json` is a C++20 header-only utility library for converting almost any C++ type/data structure to a [nlohmann::json](https://github.com/nlohmann/json) object without the need to write a custom serializer. The library uses the [boost::pfr](https://www.boost.org/doc/libs/release/libs/pfr/doc/html/index.html) library for reflection.

## Features

- toJson() Converts almost any C++ type/data structure to a nlohmann::json object
- toString() Converts almost any C++ type/data structure to a string
- Supports nested objects and arrays
- Supports nlohmann::json initializer lists that contain structs

## Limitations

- Does not support classes/structs with private sections because of boost::pfr limitations. If you want to serialize a class/struct with private members, you need to manually implement toJson() and/or toString() as public member functions of the class/struct.

## Usage

A full usage example can be found in [examples/JsonExample.cpp](src/examples/JsonExample.cpp)

Basic usage:

```cpp
#include <streamr-json/toJson.hpp>
#include <iostream>

using streamr::json::toJson;

struct MyStruct {
    int x;
    std::string y;
};

MyStruct s{1, "hello"};

// Converting a struct to a json object
nlohmann::json json = toJson(s);

// outputs: {"x":1,"y":"hello"}
std::cout << json << std::endl;

// Converting an initializer list containing a struct to a json object

nlohmann::json json2 = toJson({{"data", s}});

// outputs: {"data":{"x":1,"y":"hello"}}
std::cout << json2 << std::endl;
```

A template function that forwards a parameter to toJson() needs to define the default type of the type parameter as `streamr::json::StreamrJsonInitializerList`. This is necessary as C++ templating system is not able to deduce the type of initializer lists from the funtion call.

```cpp

#include <streamr-json/toJson.hpp>

using streamr::json::StreamrJsonInitializerList;

// Default type parameter is important here
template <typename T = StreamrJsonInitializerList>
void printMyData(T data) {
    std::cout << toJson(data) << std::endl;
}

// now this works
printMyData({{"forwarderData", s}});

```
