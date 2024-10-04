cmake_minimum_required(VERSION 3.26)

project(streamrproxyclientexample CXX)

set(CMAKE_CXX_STANDARD 26)

add_executable(publisherexample publisherexample.cpp)
target_include_directories(publisherexample PRIVATE ${CMAKE_SOURCE_DIR}/../../include)
target_link_directories(publisherexample PRIVATE ${CMAKE_SOURCE_DIR}/../../build)
target_link_libraries(publisherexample streamrproxyclient)
