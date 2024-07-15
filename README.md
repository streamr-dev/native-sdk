# Streamr Native SDK

This is the ongoing development repository for the Streamr Native SDK, a native counterpart of the [streamr-dev/network](https://github.com/your-github-username/streamr-dev/network).

## Clone the repository

```bash
git clone git@github.com:streamr-dev/native-sdk.git
```

## Install Prerequisites

```bash
cd native-sdk
source install-prerequisites.sh
```

This script will recognize the operating system type and install the prerequisites for the SDK. It will also create a `setenvs.sh` file in the root directory of the repository that you can use to set the correct environment variables for the SDK when you resume development in a new terminal.

## Install all the dependencies and build the SDK

```bash
./install.sh
```

`install.sh` does the following:

1. Loops through packages listed in `MonorepoPackages.cmake`, and builds each using `cmake` and `make`. This will trigger the fetching of dependencies using `vcpkg` and the building of each package in its own `build` directory.
2. Builds the SDK in the root directory using `cmake` and `make`. This will trigger the building of the SDK in the `build` folder of the root directory. This is needed for VSCode and its extensions to work correctly.

## Cleaning the build folders

```bash
./clean.sh
```

This script loops through packages listed in `MonorepoPackages.cmake` and cleans the build folders of each package except for the `.gitignore` files. It also cleans the root build folder.

## Resuming development in a new terminal

To resume development in a new terminal, run the following command:

```bash
source setenvs.sh
```

# Troubleshooting

## Installation
Apple Silicon ARM: If you encounter CMake errors early in `./install.sh` step (installing vcpkg), check your brew version and brew directories. If your brew was installed originally on Intel Mac, the homebrew directory is under `/usr/local`, while under ARM it is under `/opt/homebrew`. [You may need to re-install brew](https://apple.stackexchange.com/questions/410825/apple-silicon-port-all-homebrew-packages-under-usr-local-opt-to-opt-homebrew).

## Linting
Linter is run when doing a git commit. If you get lots of errors like such as this one:

```
packages/streamr-json/include/streamr-json/JsonBuilder.hpp:5:10: Error: In included file: a space is required between consecutive right angle brackets (use '> >') [two_right_angle_brackets_need_space]
```

the problem could be outdated clangd version, or perhaps the clangd is not found from homebrew's path. Check `clangd --version` (should be 18.1.* or newer) and `which clangd` (should be `/opt/homebrew/bin/clangd`). If clangd is found in `/usr/bin/clangd`, check that your path has `/opt/homebrew/bin` before `/usr/bin` (this should be set if your .zprofile or .zshrc).
