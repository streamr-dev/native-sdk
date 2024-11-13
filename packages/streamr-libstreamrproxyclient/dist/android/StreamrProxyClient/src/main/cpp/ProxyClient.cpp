//
// Created by Santtu Rantanen on 28.8.2024.
//

#include <jni.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <android/log.h>
#include "streamrproxyclient.h"

#define LOG_TAG "YourTag"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

JNIEXPORT jlong JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientNew(
        JNIEnv *env, jclass, jstring jOwnEthereumAddress, jstring jStreamPartId) {
    const char *ownEthereumAddress =
            env->GetStringUTFChars(jOwnEthereumAddress, 0);
    const char *streamPartId = env->GetStringUTFChars(jStreamPartId, 0);

    const ProxyResult *proxyResult = nullptr;
    uint64_t clientHandle =
            proxyClientNew(&proxyResult, ownEthereumAddress, streamPartId);

    env->ReleaseStringUTFChars(jOwnEthereumAddress, ownEthereumAddress);
    env->ReleaseStringUTFChars(jStreamPartId, streamPartId);
    // Handle errors if any
    if (proxyResult && proxyResult->numErrors > 0) {
        const Error &error = proxyResult->errors[0];

        // Find the StreamrError class and appropriate subclass based on error code
        jclass streamrErrorClass = nullptr;
        if (strcmp(error.code, ERROR_INVALID_ETHEREUM_ADDRESS) == 0) {
            streamrErrorClass = env->FindClass(
                    "network/streamr/proxyclient/StreamrError$InvalidEthereumAddress");
        } else if (strcmp(error.code, ERROR_INVALID_STREAM_PART_ID) == 0) {
            streamrErrorClass = env->FindClass(
                    "network/streamr/proxyclient/StreamrError$InvalidStreamPartId");
        } else {
            // Default to UnknownError for unrecognized error codes
            streamrErrorClass = env->FindClass(
                    "network/streamr/proxyclient/StreamrError$UnknownError");
        }

        if (streamrErrorClass == nullptr) {
            proxyClientResultDelete(proxyResult);
            return 0; // Class not found
        }

        // Get the constructor for the error class
        jmethodID errorConstructor = env->GetMethodID(streamrErrorClass, "<init>",
                                                      "(Ljava/lang/String;)V");
        jstring message = env->NewStringUTF(error.message);

        // Create the StreamrError instance
        jobject streamrError = env->NewObject(streamrErrorClass, errorConstructor, message);

        // Find ProxyClientException class and its constructor
        jclass exceptionClass = env->FindClass("network/streamr/proxyclient/ProxyClientException");
        if (exceptionClass == nullptr) {
            proxyClientResultDelete(proxyResult);
            return 0;
        }

        // Get the constructor that takes StreamrError
        jmethodID constructor = env->GetMethodID(
                exceptionClass,
                "<init>",
                "(Lnetwork/streamr/proxyclient/StreamrError;)V");

        // Create and throw the exception
        jobject exception = env->NewObject(exceptionClass, constructor, streamrError);
        env->Throw((jthrowable) exception);

        // Cleanup
        env->DeleteLocalRef(message);
        env->DeleteLocalRef(streamrError);
        proxyClientResultDelete(proxyResult);
        return 0;
    }
/*
    // Handle errors if any
    if (proxyResult && proxyResult->numErrors > 0) {
        // Get the first error (in this case, there should only be one)
        const Error& error = proxyResult->errors[0];

        // Find the ProxyClientException class
        jclass exceptionClass =
            env->FindClass("network/streamr/proxyclient/ProxyClientException");
        if (exceptionClass == nullptr) {
            proxyClientResultDelete(proxyResult);
            return 0; // Class not found
        }

        // Get the constructor
        jmethodID constructor = env->GetMethodID(
            exceptionClass,
            "<init>",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

        // Convert C strings to Java strings
        jstring message = env->NewStringUTF(error.message);
        jstring code = env->NewStringUTF(error.code);
        jstring proxyUrl = error.proxy
            ? env->NewStringUTF(error.proxy->websocketUrl)
            : nullptr;
        jstring proxyAddress = error.proxy
            ? env->NewStringUTF(error.proxy->ethereumAddress)
            : nullptr;

        // Create and throw the exception
        jobject exception = env->NewObject(
            exceptionClass, constructor, message, code, proxyUrl, proxyAddress);
        env->Throw((jthrowable)exception);

        // Cleanup
        proxyClientResultDelete(proxyResult);
        return 0;
    }
*/
    proxyClientResultDelete(proxyResult);
    return static_cast<jlong>(clientHandle);
}

extern "C" JNIEXPORT void JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientDelete(
        JNIEnv *env, jobject /* this */, jlong handle) {
    const ProxyResult *proxyResult = nullptr;
    proxyClientDelete(&proxyResult, static_cast<uint64_t>(handle));

    if (proxyResult != nullptr) {
        // Clean up the C ProxyResult
        proxyClientResultDelete(proxyResult);
    }
}

extern "C" JNIEXPORT jobject JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientConnect(
        JNIEnv *env, jobject /* this */, jlong handle, jobject jProxies) {
    // Get List size
    jclass listClass = env->GetObjectClass(jProxies);
    jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
    jint numProxies = env->CallIntMethod(jProxies, sizeMethod);

    // Get List.get method
    jmethodID getMethod =
            env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    // Create C proxy array
    std::vector<Proxy> proxies(numProxies);

    // Get StreamrProxyAddress class and methods
    jclass proxyAddressClass =
            env->FindClass("network/streamr/proxyclient/StreamrProxyAddress");
    jmethodID getWebsocketUrl = env->GetMethodID(
            proxyAddressClass, "getWebsocketUrl", "()Ljava/lang/String;");
    jmethodID getEthereumAddress = env->GetMethodID(
            proxyAddressClass, "getEthereumAddress", "()Ljava/lang/String;");

    // Store strings to ensure they stay alive
    std::vector<std::string> websocketUrls(numProxies);
    std::vector<std::string> ethereumAddresses(numProxies);

    for (jint i = 0; i < numProxies; i++) {
        // Get proxy object from List
        jobject jProxy = env->CallObjectMethod(jProxies, getMethod, i);

        // Get URL and address
        jstring jWebsocketUrl =
                (jstring) env->CallObjectMethod(jProxy, getWebsocketUrl);
        jstring jEthereumAddress =
                (jstring) env->CallObjectMethod(jProxy, getEthereumAddress);

        const char *websocketUrl =
                env->GetStringUTFChars(jWebsocketUrl, nullptr);
        const char *ethereumAddress =
                env->GetStringUTFChars(jEthereumAddress, nullptr);

        // Store strings
        websocketUrls[i] = websocketUrl;
        ethereumAddresses[i] = ethereumAddress;

        // Set up proxy
        proxies[i].websocketUrl = websocketUrls[i].c_str();
        proxies[i].ethereumAddress = ethereumAddresses[i].c_str();

        // Release Java strings
        env->ReleaseStringUTFChars(jWebsocketUrl, websocketUrl);
        env->ReleaseStringUTFChars(jEthereumAddress, ethereumAddress);

        // Clean up local refs
        env->DeleteLocalRef(jWebsocketUrl);
        env->DeleteLocalRef(jEthereumAddress);
        env->DeleteLocalRef(jProxy);
    }

    // Call C-API
    const ProxyResult *proxyResult = nullptr;
    uint64_t numConnected = proxyClientConnect(
            &proxyResult,
            static_cast<uint64_t>(handle),
            proxies.data(),
            static_cast<uint64_t>(numProxies));

    // Convert result to Java object
    jobject result = nullptr;
    if (proxyResult != nullptr) {
        // Find the StreamrProxyResult class and its constructor
        jclass proxyResultClass =
                env->FindClass("network/streamr/proxyclient/StreamrProxyResult");
        jmethodID constructor = env->GetMethodID(
                proxyResultClass, "<init>", "(JLjava/util/List;Ljava/util/List;)V");

        // Create lists for successful and failed proxies
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID arrayListConstructor =
                env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject successfulList =
                env->NewObject(arrayListClass, arrayListConstructor);
        jobject failedList =
                env->NewObject(arrayListClass, arrayListConstructor);
        jmethodID addMethod =
                env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        // Add successful proxies
        for (uint64_t i = 0; i < proxyResult->numSuccessful; i++) {
            const Proxy &proxy = proxyResult->successful[i];
            jstring websocketUrl = env->NewStringUTF(proxy.websocketUrl);
            jstring ethereumAddress = env->NewStringUTF(proxy.ethereumAddress);

            jobject proxyAddress = env->NewObject(
                    proxyAddressClass,
                    env->GetMethodID(
                            proxyAddressClass,
                            "<init>",
                            "(Ljava/lang/String;Ljava/lang/String;)V"),
                    websocketUrl,
                    ethereumAddress);

            env->CallBooleanMethod(successfulList, addMethod, proxyAddress);

            env->DeleteLocalRef(websocketUrl);
            env->DeleteLocalRef(ethereumAddress);
            env->DeleteLocalRef(proxyAddress);
        }

        // Add failed proxies
        jclass proxyErrorClass =
                env->FindClass("network/streamr/proxyclient/StreamrProxyError");
        jclass streamrErrorClass =
                env->FindClass("network/streamr/proxyclient/StreamrError");

        for (uint64_t i = 0; i < proxyResult->numErrors; i++) {
            const Error &error = proxyResult->errors[i];

            // Create StreamrError
            jstring message = env->NewStringUTF(error.message);
            jobject streamrError;

            // Create appropriate StreamrError subclass based on error code
            if (strcmp(error.code, ERROR_INVALID_PROXY_URL) == 0) {
                jclass errorClass = env->FindClass(
                        "network/streamr/proxyclient/StreamrError$InvalidProxyUrl");
                jmethodID errorConstructor = env->GetMethodID(
                        errorClass, "<init>", "(Ljava/lang/String;)V");
                streamrError =
                        env->NewObject(errorClass, errorConstructor, message);
            } else if (strcmp(error.code, ERROR_PROXY_CONNECTION_FAILED) == 0) {
                jclass errorClass = env->FindClass(
                        "network/streamr/proxyclient/StreamrError$ProxyConnectionFailed");
                jmethodID errorConstructor = env->GetMethodID(
                        errorClass, "<init>", "(Ljava/lang/String;)V");
                streamrError =
                        env->NewObject(errorClass, errorConstructor, message);
            } else if (
                    strcmp(error.code, ERROR_INVALID_ETHEREUM_ADDRESS) == 0) {
                jclass errorClass = env->FindClass(
                        "network/streamr/proxyclient/StreamrError$InvalidEthereumAddress");
                jmethodID errorConstructor = env->GetMethodID(
                        errorClass, "<init>", "(Ljava/lang/String;)V");
                streamrError =
                        env->NewObject(errorClass, errorConstructor, message);
            } else {
                jclass errorClass = env->FindClass(
                        "network/streamr/proxyclient/StreamrError$UnknownError");
                jmethodID errorConstructor = env->GetMethodID(
                        errorClass, "<init>", "(Ljava/lang/String;)V");
                streamrError =
                        env->NewObject(errorClass, errorConstructor, message);
            }

            // Create ProxyAddress for the error
            jstring websocketUrl = env->NewStringUTF(error.proxy->websocketUrl);
            jstring ethereumAddress =
                    env->NewStringUTF(error.proxy->ethereumAddress);
            jobject errorProxy = env->NewObject(
                    proxyAddressClass,
                    env->GetMethodID(
                            proxyAddressClass,
                            "<init>",
                            "(Ljava/lang/String;Ljava/lang/String;)V"),
                    websocketUrl,
                    ethereumAddress);

            // Create ProxyError
            jobject proxyError = env->NewObject(
                    proxyErrorClass,
                    env->GetMethodID(
                            proxyErrorClass,
                            "<init>",
                            "(Lnetwork/streamr/proxyclient/StreamrError;Lnetwork/streamr/proxyclient/StreamrProxyAddress;)V"),
                    streamrError,
                    errorProxy);

            env->CallBooleanMethod(failedList, addMethod, proxyError);

            env->DeleteLocalRef(message);
            env->DeleteLocalRef(streamrError);
            env->DeleteLocalRef(websocketUrl);
            env->DeleteLocalRef(ethereumAddress);
            env->DeleteLocalRef(errorProxy);
            env->DeleteLocalRef(proxyError);
        }

        // Create final StreamrProxyResult
        result = env->NewObject(
                proxyResultClass,
                constructor,
                static_cast<jlong>(numConnected),
                successfulList,
                failedList);

        // Clean up
        env->DeleteLocalRef(successfulList);
        env->DeleteLocalRef(failedList);
    }

    // Clean up C ProxyResult
    if (proxyResult != nullptr) {
        proxyClientResultDelete(proxyResult);
    }

    return result;
}

extern "C" JNIEXPORT jobject JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientPublish(
        JNIEnv *env,
        jobject /* this */,
        jlong handle,
        jstring jContent,
        jstring jEthereumPrivateKey) {
    LOGD("Starting proxyClientPublish with handle: %ld", handle);

    // Convert Java strings to C strings
    const char *content = env->GetStringUTFChars(jContent, nullptr);
    const char *ethereumPrivateKey = jEthereumPrivateKey
                                     ? env->GetStringUTFChars(jEthereumPrivateKey, nullptr)
                                     : nullptr;

    // Get content length
    jsize contentLength = env->GetStringUTFLength(jContent);
    LOGD("Content length: %d", contentLength);

    // Call C-API
    const ProxyResult *proxyResult = nullptr;
    uint64_t numPublished = proxyClientPublish(
            &proxyResult,
            static_cast<uint64_t>(handle),
            content,
            static_cast<uint64_t>(contentLength),
            ethereumPrivateKey);

    // Create Java result object
    jobject result = nullptr;

    if (proxyResult != nullptr) {
        // Find the StreamrProxyResult class and its constructor
        jclass proxyResultClass =
                env->FindClass("network/streamr/proxyclient/StreamrProxyResult");
        jmethodID constructor = env->GetMethodID(
                proxyResultClass, "<init>", "(JLjava/util/List;Ljava/util/List;)V");

        // Create lists for successful and failed proxies
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID arrayListConstructor =
                env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject successfulList =
                env->NewObject(arrayListClass, arrayListConstructor);
        jobject failedList =
                env->NewObject(arrayListClass, arrayListConstructor);

        // Add successful proxies
        if (proxyResult->numSuccessful > 0) {
            jclass proxyAddressClass = env->FindClass(
                    "network/streamr/proxyclient/StreamrProxyAddress");
            jmethodID proxyAddressConstructor = env->GetMethodID(
                    proxyAddressClass,
                    "<init>",
                    "(Ljava/lang/String;Ljava/lang/String;)V");
            jmethodID addMethod = env->GetMethodID(
                    arrayListClass, "add", "(Ljava/lang/Object;)Z");

            for (uint64_t i = 0; i < proxyResult->numSuccessful; i++) {
                const Proxy &proxy = proxyResult->successful[i];
                jstring websocketUrl = env->NewStringUTF(proxy.websocketUrl);
                jstring ethereumAddress =
                        env->NewStringUTF(proxy.ethereumAddress);

                jobject proxyAddress = env->NewObject(
                        proxyAddressClass,
                        proxyAddressConstructor,
                        websocketUrl,
                        ethereumAddress);
                env->CallBooleanMethod(successfulList, addMethod, proxyAddress);

                env->DeleteLocalRef(websocketUrl);
                env->DeleteLocalRef(ethereumAddress);
                env->DeleteLocalRef(proxyAddress);
            }
        }

        // Add failed proxies
        if (proxyResult->numErrors > 0) {
            jclass proxyAddressClass = env->FindClass(
                    "network/streamr/proxyclient/StreamrProxyAddress");
            jclass proxyErrorClass =
                    env->FindClass("network/streamr/proxyclient/StreamrProxyError");
            jmethodID addMethod = env->GetMethodID(
                    arrayListClass, "add", "(Ljava/lang/Object;)Z");

            for (uint64_t i = 0; i < proxyResult->numErrors; i++) {
                const Error &error = proxyResult->errors[i];
                jstring message = env->NewStringUTF(error.message);
                jobject streamrError;

                // Create appropriate StreamrError based on error code
                if (strcmp(error.code, ERROR_PROXY_BROADCAST_FAILED) == 0) {
                    jclass errorClass = env->FindClass(
                            "network/streamr/proxyclient/StreamrError$ProxyConnectionFailed");
                    jmethodID errorConstructor = env->GetMethodID(
                            errorClass, "<init>", "(Ljava/lang/String;)V");
                    streamrError =
                            env->NewObject(errorClass, errorConstructor, message);
                } else {
                    jclass errorClass = env->FindClass(
                            "network/streamr/proxyclient/StreamrError$UnknownError");
                    jmethodID errorConstructor = env->GetMethodID(
                            errorClass, "<init>", "(Ljava/lang/String;)V");
                    streamrError =
                            env->NewObject(errorClass, errorConstructor, message);
                }

                // Create proxy address for the error
                jstring websocketUrl =
                        env->NewStringUTF(error.proxy->websocketUrl);
                jstring ethereumAddress =
                        env->NewStringUTF(error.proxy->ethereumAddress);
                jobject proxyAddress = env->NewObject(
                        proxyAddressClass,
                        env->GetMethodID(
                                proxyAddressClass,
                                "<init>",
                                "(Ljava/lang/String;Ljava/lang/String;)V"),
                        websocketUrl,
                        ethereumAddress);

                // Create and add ProxyError
                jobject proxyError = env->NewObject(
                        proxyErrorClass,
                        env->GetMethodID(
                                proxyErrorClass,
                                "<init>",
                                "(Lnetwork/streamr/proxyclient/StreamrError;Lnetwork/streamr/proxyclient/StreamrProxyAddress;)V"),
                        streamrError,
                        proxyAddress);
                env->CallBooleanMethod(failedList, addMethod, proxyError);

                // Cleanup
                env->DeleteLocalRef(message);
                env->DeleteLocalRef(streamrError);
                env->DeleteLocalRef(websocketUrl);
                env->DeleteLocalRef(ethereumAddress);
                env->DeleteLocalRef(proxyAddress);
                env->DeleteLocalRef(proxyError);
            }
        }

        // Create final result
        result = env->NewObject(
                proxyResultClass,
                constructor,
                static_cast<jlong>(numPublished),
                successfulList,
                failedList);

        // Cleanup lists
        env->DeleteLocalRef(successfulList);
        env->DeleteLocalRef(failedList);
    }

    // Cleanup C strings
    env->ReleaseStringUTFChars(jContent, content);
    if (ethereumPrivateKey) {
        env->ReleaseStringUTFChars(jEthereumPrivateKey, ethereumPrivateKey);
    }

    // Cleanup ProxyResult
    if (proxyResult != nullptr) {
        proxyClientResultDelete(proxyResult);
    }

    return result;
}
}