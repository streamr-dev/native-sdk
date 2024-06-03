#!/bin/bash

clang-tidy include/streamr-logger/Logger.hpp -p build -- -isystem build/vcpkg_installed
