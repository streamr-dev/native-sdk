# Streamr Native SDK

This is the ongoing development repository for the Streamr Native SDK, a native counterpart of the [streamr-dev/network](https://github.com/your-github-username/streamr-dev/network).

## Getting started

### Recommended development environment

* [Cursor editor](https://cursor.sh/) with a copilot++ subscription 
* Recommended Cursor extensions:
  * C/C++ Extension Pack (includes clang-format support with shift+option+f)
  * clangd (better highlighting and auto-completion than IntelliSense)
  * CMake
  * CMake Tools
  * CMake Test Explorer (allows running individual test cases from UI)
  * Test Explorer UI
  * Better C++ Syntax
  * vscode-proto3
  * Doxygen Documentation Generator
  
  

### Clone the repository

```bash
git clone git@github.com:streamr-dev/native-sdk.git
```

### Install Prerequisities

```bash
cd native-sdk
source install-prerequisities.sh
```

This script will recognize the operating system type and install the prerequisities for the SDK. It will also create a `setenvs.sh` file in the root directory of the repository that you can use to set the correct environment variables for the SDK when you resume development in a new terminal.

### Install all the dependencies and build the SDK for MacOS and Linux

```bash
./install.sh
```

The `install.sh` script supports an optional `--prod` parameter. When this parameter is provided, the script will build the SDK in Release mode. If the `--prod` parameter is not provided, the script will default to building the SDK in Debug mode.

`install.sh` does the following:

1. Loops through packages listed in `MonorepoPackages.cmake`, and builds each using `cmake` and `make`. This will trigger the fetching of dependencies using `vcpkg` and the building of each package in its own `build` directory.
2. Builds the SDK in the root directory using `cmake` and `make`. This will trigger the building of the SDK in the `build` folder of the root directory. This is needed for VSCode and its extensions to work correctly.

### Install all the dependencies and build the SDK for iOS

```bash
./install.sh --ios
```

This command installs the Streamr XCFramework and an example iOS app, LocationShare, which uses the XCFramework. The Streamr XCFramework includes the static library of the Streamr native-sdk. When it comes to using C++ libraries in iOS apps, static libraries are generally recommended over dynamic libraries.

The Streamr XCFramework is located in `dist/ios/streamr.xcframework` and can be used in an iOS app by simply dragging and dropping it into the Xcode project.

The example iOS app can be found in `packages/streamr-libstreamrproxyclient/examples/ios/LocationShare`.

### Install all the dependencies and build the SDK for Android 

```bash
./install.sh --android
```

This command installs the shared library libstreamrproxyclient.so and an example Android app, LocationShare, which uses the shared library.

The example Android app can be found in `packages/streamr-libstreamrproxyclient/examples/android/LocationShare`.

### Cleaning the build folders

```bash
./clean.sh
```

This script loops through packages listed in `MonorepoPackages.cmake` and cleans the build folders of each package except for the `.gitignore` files. It also cleans the root build folder.

### Resuming development in a new terminal

To resume development in a new terminal, run the following command:

```bash
source setenvs.sh
```

## Implementation details

The Streamr Native SDK is implemented as a monorepo using [CMake](https://cmake.org/) as a build system. The monorepo design makes it possible that

* Each package can be built and tested independently.
* Each package can be easily split into its own repository.

On the other hand,

* The whole monorepo can be opened in an IDE like Cursor and with all the features of the IDE such as code navigation, code completion, etc working accross packages.
* The whole monorepo can be built and tested together (especially in CI).

### Monorepo root directory structure

The root directory of the monorepo has the following structure:

#### GIT submodules
The Streamr Native SDK monorepo has two GIT submodules at its root:

* `vcpkg` - [vcpkg package manager by Microsoft](https://github.com/microsoft/vcpkg) (installing as a submodule is the recommended way of installation)
* `clangd-tidy` - [clangd-tidy](https://github.com/lljbash/clangd-tidy) A fast variant of the clang-tidy linter for C++. (not available as a brew or apt package)

#### Directories
* `build` - the main build directory for the whole monorepo.
* `packages` - the directory containing all the packages in the monorepo. Each package should be placed under packages in its own directory prefixed with `streamr-`.

#### CMake files
* `MonorepoPackages.cmake` - a CMake file that lists all the packages in the monorepo. **The order of the packages in this file is important.** The packages are built in the order of the list and the dependencies between the packages are resolved based on the order of the packages in the list.
* `homebrewClang.cmake` - a CMake file that takes the latest version of the Clang compiler and associated tools into use in the build on macOS.
* `CMakeLists.txt` - the main CMake file for the whole monorepo. Includes all the packages listed in `MonorepoPackages.cmake` into the build using `add_subdirectory()` of CMAKE.

#### Scripts
| Script       | Ts equivalent | Description                                                                 |
|--------------|---------------|-----------------------------------------------------------------------------|
| `install-prerequisites.sh` | N/A           | Install development prerequisites and set necessary environment variables.  **Must be run using `source install-prerequisites.sh`** |
| `setenvs.sh` | N/A       | Set necessary environment variables for the monorepo when resuming development in a new terminal. **Must be run using `source setenvs.sh`** |
| `install.sh` | npm install           | Install all the VCPKG dependencies and build the whole monorepo. |
| `build.sh` | npm run build           | Build the whole monorepo. |
| `lint.sh` | npm run lint           | Run clangd-tidy and clang-format on the whole monorepo. |
| `test.sh` | npm run test           | Run the tests of the whole monorepo. |
| `clean.sh`   | npm run clean         | Clean the build folders of all the packages in the monorepo. |
| `merge-dependencies.sh` | N/A       | A helper script called by install.sh to merge the VCPKG dependencies of the monorepo packages to the root vcpkg.json **must not be invoked by the user** |
| `iostest.sh`| N/A                   | Run selected unit tests in iOS Device (In MacOS by default). Unit tests can be selected by adding tests (Drag and drop) to the App iOSUnitTesting. |


#### Configuration files
| File       | Description                                                                 |
|--------------|-----------------------------------------------------------------------------|
| `.vcpkg.json` | Vcpkg dependencies of the whole monorepo. **This file is automatically generated by the `install.sh` script and should not be edited manually.** |
| `.clang-tidy` | Configuration file for the clangd-tidy high-level linter. |
| `.clang-format` | Configuration file for the clang-format low-level linter. |
| `.clangd` | Configuration file for the clangd plugin of Cursor/VSCode. Controls syntax highlighting, code completion, etc. |
| `.githooks/pre-commit` | A githook script that runs lint.sh before a commit is made. This hook is added to the git configuration at the end of the install-prerequisites.sh script. |
| `.github/*` | Configuration files for the GitHub Actions CI/CD. |

### Package directory structure

Monorepo packages located under `packages/streamr-*` have the following structure:

#### Directories

* `include` - the public `*.hpp` header files of the package.
* `src` - the `*.cpp` source files of the package.
* `test` - the `*.cpp` test files of the package.

#### CMake files

* `homebrewClang.cmake` - a CMake file that takes the latest version of the Clang compiler and associated tools into use in the build on macOS. **This file is exactly the same as the one in the root directory.**
* `monorepoPackage.cmake` - a CMake file that detects whether the package is being built in the monorepo or in its own repository. If the package is being built in the monorepo, monorepoPackage.cmake instructs CMake to find the monorepo dependencies listed in `.vcpkg.json` from the filesystem. Otherwise, it the monopero dependencies will be installed as regular vcpkg dependencies of the package. **This file is exactly the same in all the packages in the monorepo.**
* `CMakeLists.txt` - the main CMake file for the package. 
Exports the package as `build/<package-name>-config.cmake` file that the `monorepoPackage.cmake` files of other monorepo packages use to link against the package.

#### Scripts

| Script       | Ts equivalent | Description                                                                 |
|--------------|---------------|-----------------------------------------------------------------------------|
| `install.sh` | npm install           | Install all the VCPKG dependencies and build the package. |
| `build.sh` | npm run build           | Build the package. |
| `test.sh` | npm run test           | Run the tests of the package. |
| `clean.sh`   | npm run clean         | Clean the build folder of the package. |

### Configuration files

* `vcpkg.json` - standard vcpkg dependencies file of the package. Additionally, contains the standard-compliant "features.monorepo.dependencies" array that lists the dependencies of the package in the monorepo. If the package is being build outside of the monorepo, vcpkg will install also these dependencies from the vcpkg repositories.

## Troubleshooting

### Installation
Apple Silicon ARM: If you encounter CMake errors early in `./install.sh` step (installing vcpkg), check your brew version and brew directories. If your brew was installed originally on Intel Mac, the homebrew directory is under `/usr/local`, while under ARM it is under `/opt/homebrew`. [You may need to re-install brew](https://apple.stackexchange.com/questions/410825/apple-silicon-port-all-homebrew-packages-under-usr-local-opt-to-opt-homebrew).

### Linting

Linter is run when doing a git commit. If you get lots of errors like such as this one:

```
packages/streamr-json/include/streamr-json/JsonBuilder.hpp:5:10: Error: In included file: a space is required between consecutive right angle brackets (use '> >') [two_right_angle_brackets_need_space]
```

the problem could be outdated clangd version, or perhaps the clangd is not found from homebrew's path. Check `clangd --version` (should be 18.1.* or newer) and `which clangd` (should be `/opt/homebrew/bin/clangd`). If clangd is found in `/usr/bin/clangd`, check that your path has `/opt/homebrew/bin` before `/usr/bin` (this should be set if your .zprofile or .zshrc).
