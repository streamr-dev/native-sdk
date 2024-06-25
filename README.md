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
