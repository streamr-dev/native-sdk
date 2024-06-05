# Streamr Native SDK

This is the ongoing development repository for the Streamr Native SDK, a native counterpart of the [streamr-dev/network](https://github.com/your-github-username/streamr-dev/network).


## Prerequisites

### MAC

```bash
 xcode-select --install
 brew install jq
 brew install llvm
 ln -s /opt/homebrew/Cellar/llvm/18.1.6/bin/clang-format /opt/homebrew/bin/clang-format
 ln -s /opt/homebrew/Cellar/llvm/18.1.6/bin/clangd /opt/homebrew/bin/clangd
 brew install trunk
```

### Ubuntu

```bash
sudo apt-add-repository http://apt.llvm.org/jammy/llvm-toolchain-jammy main
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build jq clang-format clang-tidy
```
## Clone the repository

**Note:** When cloning the repository, it is crucial to use the `--recursive` flag. This flag is necessary for initializing and updating the submodules that are part of this repository. Without it, the submodules will not be downloaded, leading to missing dependencies and build failures.

```bash
git clone --recursive git@github.com:streamr-dev/native-sdk.git
```

## Install VCPKG

```bash
source install-vcpkg.sh
```

This command will:

* Update the vcpkg submodule
* Run `bootstrap-vcpkg.sh -disableMetrics` in the vcpkg submodule directory
* Set env variable `VCPKG_ROOT` to point to the vcpkg submodule directory
* Add `vcpkg` directory to the `PATH`
* Add or update the setting of the `VCPKG_ROOT` and `PATH` in the `.zshrc` or `.bashrc` file

## Build the SDK

```bash
./build.sh
``` 

`build.sh` does the following:

1. Loops through packages listed in `MonorepoPackages.cmake`, and builds each using `cmake` and `make`. This will trigger the fetching of dependencies using `vcpkg` and the building of each package in its own `build` directory.
2. Builds the SDK in the root directory using `cmake` and `make`. This will trigger the building of the SDK in the `build` folder of the root directory.

## Cleaning the build folders

```bash
./clean.sh
```

This script loops through packages listed in `MonorepoPackages.cmake` and cleans the build folders of each package except for the `.gitignore` files. It also cleans the root build folder.





