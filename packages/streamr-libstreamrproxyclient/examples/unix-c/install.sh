#!/bin/bash

set -e

cd build && cmake .. && cmake --build . && cmake --install . && cd ..
