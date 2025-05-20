include(":app")
include (":StreamrProxyClient")
project(":StreamrProxyClient").projectDir = File("../../../dist/android-library-module/StreamrProxyClient")

pluginManagement {
    repositories {
        google {
            content {
                includeGroupByRegex("com\\.android.*")
                includeGroupByRegex("com\\.google.*")
                includeGroupByRegex("androidx.*")
            }
        }
        mavenCentral()
        maven(url = "https://jitpack.io")
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        maven(url = "https://jitpack.io")
        maven(url = "https://raw.github.com/saki4510t/libcommon/master/repository")
    }
}

rootProject.name = "StreamrVideo"
 