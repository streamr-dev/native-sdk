# Streamr Android Library Development

Development workspace for the StreamrProxyClient Android library module.

## Project Structure
```
android-library-module/
├── StreamrProxyClient/         # Main library module
│   ├── src/                    # Source code
│   │   ├── main/              # Main source set
│   │   │   ├── cpp/           # Native code
│   │   │   ├── java/          # Kotlin/Java code
│   │   │   └── AndroidManifest.xml
│   │   ├── test/              # Unit tests
│   │   └── androidTest/       # Instrumented tests
│   ├── libs/                  # Native libraries
│   ├── build.gradle.kts       # Main build file
│   └── build.test.gradle.kts  # Test build file
├── switch_build.sh            # Build configuration switcher
└── test.sh                    # Test runner script
```

### Requirements
- Android Studio Arctic Fox or newer
- JDK 17
- Android SDK 34
- NDK 27.1.12297006 
- CMake 3.22.1

### Initial Setup
1. Clone the repository
2. Open the project in Android Studio
3. Sync project with Gradle files

### Build Configuration
The project uses two build configurations:
- `build.gradle.kts` - For production builds
- `build.test.gradle.kts` - For testing

## Switch between configurations using:
Switch to test configuration
./switch_build.sh test
Switch to production configuration
./switch_build.sh

### Run Tests
- Terminal: ./test.sh

The test script will:
1. Switch to test configuration
2. Run instrumented tests
3. Switch back to production configuration 
