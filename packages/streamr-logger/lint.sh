#!/bin/bash

clang-tidy include/streamr-logger/Logger.hpp -p build -VV -- -isystem build/vcpkg_installed --extra-arg=-xc++ --extra-arg=-std=c++23
