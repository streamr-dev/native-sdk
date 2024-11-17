
buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath("com.android.tools.build:gradle:8.2.0")
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:1.9.20")
    }
}

plugins {
    id("com.android.library") version "8.2.0"
    id("org.jetbrains.kotlin.android") version "1.9.20"
}


tasks.withType<Test> {
    testLogging {
        events("passed", "skipped", "failed")
        showStandardStreams = true
    }
}

android {
    namespace = "network.streamr.proxyclient"
    compileSdk = 34
    ndkVersion = "27.1.12297006"  // Specify the NDK version


    packaging {
        jniLibs {
            pickFirsts.add("libs/arm64-v8a/libstreamrproxyclient.so")
        }
    }


    defaultConfig {
        minSdk = 24

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")
        externalNativeBuild {
            cmake {
                arguments(
                    "-DSTREAMR_LIB_ROOT=${projectDir}/../../../../../../build"
                )
            }
        }
        /*
        externalNativeBuild {
            cmake {
                cppFlags += ""
            }
        }
*/
        ndk {
            // Specifies the ABI configurations of your native
            // libraries Gradle should build and package with your app.
            abiFilters += "arm64-v8a"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("libs")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    // testImplementation "org.jetbrains.kotlin:kotlin-test-junit:$kotlin_version")
    testImplementation ("org.jetbrains.kotlin:kotlin-test-junit:2.0.21")
    //  testImplementation(kotlin("test"))
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
    //  testImplementation("org.jetbrains.kotlin:kotlin-test:1.9.0")
    // testImplementation("org.jetbrains.kotlin:kotlin-test-junit:1.9.0")
}
