#ifndef STREAMR_JSON_TEST_UNIT_WEATHERDATA_HPP
#define STREAMR_JSON_TEST_UNIT_WEATHERDATA_HPP

#include <ctime>
#include <map>
#include <string>
#include <vector>

struct TemperatureReading {
    double temperature;
    std::time_t timestamp;
};

struct DataSample {
    std::string locality;
    std::vector<TemperatureReading> temperatures;
};
struct WeatherData {
    int dataId;
    std::string dataLabel;
    std::map<std::string, std::vector<DataSample>> dataByCountry;
};

struct WeatherDataSmartPointers {
    int dataId;
    std::shared_ptr<std::string> dataLabel;
    std::map<std::string, std::vector<std::shared_ptr<DataSample>>>
        dataByCountry;
};

struct WeatherDataRegularPointers {
    int dataId;
    std::string* dataLabel;
    std::map<std::string, std::vector<DataSample*>> dataByCountry;
};

#endif
