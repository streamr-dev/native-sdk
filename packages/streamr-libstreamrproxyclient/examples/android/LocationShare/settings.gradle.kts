include(":app")
include (":StreamrProxyClient")
project(":StreamrProxyClient").projectDir = File("../../../dist/android-library-module/StreamrProxyClient")

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "LocationShare"
