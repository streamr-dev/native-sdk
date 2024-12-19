# Streamr Logger

Streamr Logger is a header-only C++ library for logging messages in the Streamr Native SDK. It aims to offer similar API and features to the [Logger class](https://github.com/streamr-dev/network/blob/main/packages/utils/src/Logger.ts) of the typescript version of the Streamr Network.

## Improvements compared to the typescript version

- Log messages contain the file name and line number where the log message was made instead of the mere module name.
- A static Logger instance can be used instead of creating a new instance for each file, as C++ can deduce the file name of the file where the logger is used at compile time.
- Log level can be set by [category](https://github.com/facebook/folly/blob/main/folly/logging/docs/LogCategories.md) which means that the log level can be set for a specific file or for a specific part of the codebase.
- contextBindings and metadata can be of any type (except classes and structs with private sections) 

## Limitations compared to the typescript version

- contextBindings and metadata are printed with same color as the log message because Folly formatter does not make the distinction between them.
- No JSON output option

## Supported env variables

- `LOG_COLORS=false` - Disable color output.
- `LOG_LEVEL=<level>` - Set the default log level. 
- `LOG_LEVEL_<category>=<level>` - Set the log level for a specific category. For example, if your source code is organized in the filesystem as `/root/src/packages/client/[subdirectories of client]` and you set `LOG_LEVEL_client=warn`, the log level for all files in `[subdirectories of client]` will be set to `warn`.

The level can be one of the following: `trace`, `debug`, `info`, `warn`, `error`, `fatal`.


## Usage

A complete usage example can be found in [src/examples/LoggerExample.cpp](src/examples/LoggerExample.cpp).

Using the static Logger functions:

```cpp
#include <streamr-logger/SLogger.hpp>

using SLogger = streamr::logger::SLogger;

SLogger::info("Hello, world!");
```

Using a local Logger instance with contextBindings

```cpp
#include <streamr-logger/Logger.hpp>

using Logger = streamr::logger::Logger;

Logger logger("my-logger");
logger.info("Hello, world!");
```

Logging with metadata given as JSON that contains a struct

```cpp
#include <streamr-logger/SLogger.hpp>

using SLogger = streamr::logger::SLogger;

struct MyDataStruct {
    std::string name;
    int value;
};

auto data = MyDataStruct{"count", 9};
SLogger::info("Program state", {{"data", data}});
```

## Implementation details

The library uses the [Folly](https://github.com/facebook/folly/blob/main/folly/logging/docs/Overview.md) logging library to handle the actual logging.   
