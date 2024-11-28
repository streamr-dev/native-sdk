#!/bin/bash

cd build && ls -A | while read FILE; do echo "\"$FILE\""; done | grep -v ".gitignore" | xargs rm -rf && cd ..
