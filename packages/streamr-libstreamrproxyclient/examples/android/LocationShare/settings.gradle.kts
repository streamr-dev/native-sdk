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

rootProject.name = "LocationShare"  // Use double quotes for strings
include(":app")  // Use include() function with parentheses
include(":StreamrProxyClient")

project(":StreamrProxyClient").projectDir = file("StreamrProxyClient")