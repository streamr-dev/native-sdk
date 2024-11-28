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

namespace {
    // Helper function to create StreamrError instance
    jobject createStreamrError(JNIEnv *env, const Error &error) {
        const char *errorClassName = nullptr;
        if (strcmp(error.code, ERROR_INVALID_ETHEREUM_ADDRESS) == 0) {
            errorClassName = "network/streamr/proxyclient/StreamrError$InvalidEthereumAddress";
        } else if (strcmp(error.code, ERROR_INVALID_STREAM_PART_ID) == 0) {
            errorClassName = "network/streamr/proxyclient/StreamrError$InvalidStreamPartId";
        } else if (strcmp(error.code, ERROR_INVALID_PROXY_URL) == 0) {
            errorClassName = "network/streamr/proxyclient/StreamrError$InvalidProxyUrl";
        } else if (strcmp(error.code, ERROR_PROXY_CONNECTION_FAILED) == 0) {
            errorClassName = "network/streamr/proxyclient/StreamrError$ProxyConnectionFailed";
        } else if (strcmp(error.code, ERROR_PROXY_BROADCAST_FAILED) == 0) {
            errorClassName = "network/streamr/proxyclient/StreamrError$ProxyBroadcastFailed";
        } else {
            errorClassName = "network/streamr/proxyclient/StreamrError$UnknownError";
        }
        jclass errorClass = env->FindClass(errorClassName);
        if (errorClass == nullptr) return nullptr;

        jmethodID constructor = env->GetMethodID(errorClass, "<init>", "(Ljava/lang/String;)V");
        jstring message = env->NewStringUTF(error.message);
        jobject streamrError = env->NewObject(errorClass, constructor, message);

        env->DeleteLocalRef(message);
        env->DeleteLocalRef(errorClass);

        return streamrError;
    }

    // Helper function to create and throw ProxyClientException
    void throwProxyClientException(JNIEnv *env, jobject streamrError) {
        jclass exceptionClass = env->FindClass("network/streamr/proxyclient/ProxyClientException");
        if (exceptionClass == nullptr) return;

        jmethodID constructor = env->GetMethodID(exceptionClass, "<init>",
                                                 "(Lnetwork/streamr/proxyclient/StreamrError;)V");

        jobject exception = env->NewObject(exceptionClass, constructor, streamrError);
        env->Throw((jthrowable) exception);

        env->DeleteLocalRef(exceptionClass);
        env->DeleteLocalRef(exception);
    }

    // Helper function to handle ProxyResult errors
    void handleProxyResultErrors(JNIEnv *env, const ProxyResult *proxyResult) {
        if (proxyResult && proxyResult->numErrors > 0) {
            jobject streamrError = createStreamrError(env, proxyResult->errors[0]);
            if (streamrError != nullptr) {
                throwProxyClientException(env, streamrError);
                env->DeleteLocalRef(streamrError);
            }
        }
    }

    // Helper function to create StreamrProxyAddress instance
    jobject createProxyAddress(JNIEnv *env, const Proxy &proxy) {
        jclass proxyAddressClass = env->FindClass(
                "network/streamr/proxyclient/StreamrProxyAddress");
        if (proxyAddressClass == nullptr) return nullptr;

        jmethodID constructor = env->GetMethodID(proxyAddressClass, "<init>",
                                                 "(Ljava/lang/String;Ljava/lang/String;)V");

        jstring websocketUrl = env->NewStringUTF(proxy.websocketUrl);
        jstring ethereumAddress = env->NewStringUTF(proxy.ethereumAddress);

        jobject proxyAddress = env->NewObject(proxyAddressClass, constructor,
                                              websocketUrl, ethereumAddress);

        env->DeleteLocalRef(websocketUrl);
        env->DeleteLocalRef(ethereumAddress);
        env->DeleteLocalRef(proxyAddressClass);

        return proxyAddress;
    }

    // Helper function to create StreamrProxyResult instance
    jobject createProxyResult(JNIEnv *env, uint64_t numConnected,
                              const std::vector<Proxy> &successful,
                              const std::vector<Error> &errors) {

        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");

        // Create lists for successful and failed proxies
        jobject successfulList = env->NewObject(arrayListClass, arrayListConstructor);
        jobject failedList = env->NewObject(arrayListClass, arrayListConstructor);

        // Add successful proxies
        for (const auto &proxy: successful) {
            jobject proxyAddress = createProxyAddress(env, proxy);
            env->CallBooleanMethod(successfulList, addMethod, proxyAddress);
            env->DeleteLocalRef(proxyAddress);
        }

        // Add failed proxies
        jclass proxyErrorClass = env->FindClass("network/streamr/proxyclient/StreamrProxyError");
        jmethodID proxyErrorConstructor = env->GetMethodID(proxyErrorClass, "<init>",
                                                           "(Lnetwork/streamr/proxyclient/StreamrError;Lnetwork/streamr/proxyclient/StreamrProxyAddress;)V");
        for (const auto &error: errors) {
            jobject streamrError = createStreamrError(env, error);
            jobject proxyAddress = createProxyAddress(env, *error.proxy);

            jobject proxyError = env->NewObject(proxyErrorClass, proxyErrorConstructor,
                                                streamrError, proxyAddress);
            env->CallBooleanMethod(failedList, addMethod, proxyError);

            env->DeleteLocalRef(streamrError);
            env->DeleteLocalRef(proxyAddress);
            env->DeleteLocalRef(proxyError);
        }

        // Create final StreamrProxyResult
        jclass proxyResultClass = env->FindClass("network/streamr/proxyclient/StreamrProxyResult");
        jmethodID resultConstructor = env->GetMethodID(proxyResultClass, "<init>",
                                                       "(JLjava/util/List;Ljava/util/List;)V");

        jobject result = env->NewObject(proxyResultClass, resultConstructor,
                                        (jlong) numConnected, successfulList, failedList);

        // Cleanup
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(successfulList);
        env->DeleteLocalRef(failedList);
        env->DeleteLocalRef(proxyErrorClass);
        env->DeleteLocalRef(proxyResultClass);
        return result;
    }
}


extern "C" {

JNIEXPORT jlong JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientNew(
        JNIEnv *env, jclass, jstring jOwnEthereumAddress, jstring jStreamPartId) {
    const char *ownEthereumAddress = env->GetStringUTFChars(jOwnEthereumAddress, 0);
    const char *streamPartId = env->GetStringUTFChars(jStreamPartId, 0);

    const ProxyResult *proxyResult = nullptr;
    uint64_t clientHandle = proxyClientNew(&proxyResult, ownEthereumAddress, streamPartId);

    env->ReleaseStringUTFChars(jOwnEthereumAddress, ownEthereumAddress);
    env->ReleaseStringUTFChars(jStreamPartId, streamPartId);

    handleProxyResultErrors(env, proxyResult);
    proxyClientResultDelete(proxyResult);

    return static_cast<jlong>(clientHandle);
}

JNIEXPORT void JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientDelete(
        JNIEnv *env, jobject, jlong handle) {
    const ProxyResult *proxyResult = nullptr;
    proxyClientDelete(&proxyResult, static_cast<uint64_t>(handle));
    proxyClientResultDelete(proxyResult);
}

JNIEXPORT jobject JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientConnect(
        JNIEnv *env, jobject, jlong handle, jobject jProxies) {
    // Get List size and method
    jclass listClass = env->GetObjectClass(jProxies);
    jint numProxies = env->CallIntMethod(jProxies,
                                         env->GetMethodID(listClass, "size", "()I"));
    jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

    // Create C proxy array
    std::vector<Proxy> proxies(numProxies);
    std::vector<std::string> websocketUrls(numProxies);
    std::vector<std::string> ethereumAddresses(numProxies);

    // Get StreamrProxyAddress class and methods
    jclass proxyAddressClass = env->FindClass("network/streamr/proxyclient/StreamrProxyAddress");
    jmethodID getWebsocketUrl = env->GetMethodID(proxyAddressClass, "getWebsocketUrl",
                                                 "()Ljava/lang/String;");
    jmethodID getEthereumAddress = env->GetMethodID(proxyAddressClass, "getEthereumAddress",
                                                    "()Ljava/lang/String;");

    // Convert Java proxies to C proxies
    for (jint i = 0; i < numProxies; i++) {
        jobject jProxy = env->CallObjectMethod(jProxies, getMethod, i);
        jstring jWebsocketUrl = (jstring) env->CallObjectMethod(jProxy, getWebsocketUrl);
        jstring jEthereumAddress = (jstring) env->CallObjectMethod(jProxy, getEthereumAddress);

        const char *websocketUrl = env->GetStringUTFChars(jWebsocketUrl, nullptr);
        const char *ethereumAddress = env->GetStringUTFChars(jEthereumAddress, nullptr);

        websocketUrls[i] = websocketUrl;
        ethereumAddresses[i] = ethereumAddress;
        proxies[i].websocketUrl = websocketUrls[i].c_str();
        proxies[i].ethereumAddress = ethereumAddresses[i].c_str();

        env->ReleaseStringUTFChars(jWebsocketUrl, websocketUrl);
        env->ReleaseStringUTFChars(jEthereumAddress, ethereumAddress);
        env->DeleteLocalRef(jWebsocketUrl);
        env->DeleteLocalRef(jEthereumAddress);
        env->DeleteLocalRef(jProxy);
    }

    // Call C-API
    const ProxyResult *proxyResult = nullptr;
    uint64_t numConnected = proxyClientConnect(&proxyResult, static_cast<uint64_t>(handle),
                                               proxies.data(), static_cast<uint64_t>(numProxies));

    // Create result object
    jobject result = nullptr;
    if (proxyResult != nullptr) {
        std::vector<Proxy> successful(proxyResult->successful,
                                      proxyResult->successful + proxyResult->numSuccessful);
        std::vector<Error> errors(proxyResult->errors,
                                  proxyResult->errors + proxyResult->numErrors);
        result = createProxyResult(env, numConnected, successful, errors);
    }

    proxyClientResultDelete(proxyResult);
    return result;
}

JNIEXPORT jobject JNICALL
Java_network_streamr_proxyclient_ProxyClientJNI_proxyClientPublish(
        JNIEnv *env, jobject, jlong handle, jstring jContent, jstring jEthereumPrivateKey) {
    LOGD("Starting proxyClientPublish with handle: %ld", handle);

    const char *content = env->GetStringUTFChars(jContent, nullptr);
    const char *ethereumPrivateKey = jEthereumPrivateKey ?
                                     env->GetStringUTFChars(jEthereumPrivateKey, nullptr) : nullptr;
    jsize contentLength = env->GetStringUTFLength(jContent);

    const ProxyResult *proxyResult = nullptr;
    uint64_t numPublished = proxyClientPublish(&proxyResult, static_cast<uint64_t>(handle),
                                               content, static_cast<uint64_t>(contentLength),
                                               ethereumPrivateKey);

    jobject result = nullptr;
    if (proxyResult != nullptr) {
        std::vector<Proxy> successful(proxyResult->successful,
                                      proxyResult->successful + proxyResult->numSuccessful);
        std::vector<Error> errors(proxyResult->errors,
                                  proxyResult->errors + proxyResult->numErrors);
        result = createProxyResult(env, numPublished, successful, errors);
    }

    env->ReleaseStringUTFChars(jContent, content);
    if (ethereumPrivateKey) {
        env->ReleaseStringUTFChars(jEthereumPrivateKey, ethereumPrivateKey);
    }

    proxyClientResultDelete(proxyResult);
    return result;
}


}