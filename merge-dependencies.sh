#!/bin/bash

# Merges the sub-package dependencies into the root vcpkg.json file

# Parse MonorepoPackages.cmake and loop through them
for package in $(cat MonorepoPackages.cmake | grep -v "set(MonorepoPackages" | grep -v ")"); do
    # Merge the dependencies into the root vcpkg.json file
    
    jq '.dependencies = (.dependencies + input.dependencies | unique)' vcpkg.json packages/$package/vcpkg.json > temp.json && mv temp.json vcpkg.json
    
    # TODO: make the python version to work to drop jq dependency
    #python -c "import json; a = json.load(open('vcpkg.json')); b = json.load(open('packages/'+$package+'/vcpkg.json')); a['dependencies'] = list(set(a.get('dependencies', []) + b.get('dependencies', []))); json.dump(a, open('temp.json', 'w'))" && mv temp.json vcpkg.json
done
