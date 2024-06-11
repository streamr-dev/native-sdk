#include <streamr-logger/Logger.hpp>

using Logger = streamr::logger::Logger;

int main() {
    int jee = 1;
    Logger logger;
    logger.log("Hello, world!");
    return 0;
}
