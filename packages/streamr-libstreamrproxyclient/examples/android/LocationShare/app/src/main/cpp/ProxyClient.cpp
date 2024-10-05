//
// Created by Santtu Rantanen on 28.8.2024.
//

//#include "ProxyClient.h"
//#include "PeerDescriptor.h"
#include "streamrproxyclient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "YourTag"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {


JNIEXPORT jlong JNICALL
Java_com_example_locationshare_ProxyClientJNI_proxyClientNew(JNIEnv *env, jclass, jstring jOwnEthereumAddress, jstring jStreamPartId) {
    const char* ownEthereumAddress = env->GetStringUTFChars(jOwnEthereumAddress, 0);
    const char* streamPartId = env->GetStringUTFChars(jStreamPartId, 0);

    Error* errors = nullptr;
    uint64_t numErrors = 0;

    uint64_t clientHandle = proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    env->ReleaseStringUTFChars(jOwnEthereumAddress, ownEthereumAddress);
    env->ReleaseStringUTFChars(jStreamPartId, streamPartId);

    if (numErrors > 0) {
        /*
        std::string errorMsg = errors[0].message;
        proxyClientFreeErrors(errors, numErrors);
        env->ThrowNew(env->FindClass("com/yourpackage/StreamrProxyClient$StreamrProxyException"), errorMsg.c_str());
         */
        return 0;
    }

    return static_cast<jlong>(clientHandle);
}

JNIEXPORT void JNICALL
Java_com_example_locationshare_ProxyClientJNI_proxyClientDelete(JNIEnv *env, jclass, jlong clientHandle) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    proxyClientDelete(&errors, &numErrors, static_cast<uint64_t>(clientHandle));

    if (numErrors > 0) {
        /*
        std::string errorMsg = errors[0].message;
        proxyClientFreeErrors(errors, numErrors);
        env->ThrowNew(env->FindClass("com/yourpackage/StreamrProxyClient$StreamrProxyException"), errorMsg.c_str());
         */
    }
}

JNIEXPORT jlong JNICALL
Java_com_example_locationshare_ProxyClientJNI_proxyClientConnect(JNIEnv *env, jclass, jlong clientHandle, jobjectArray jProxies) {
    jsize numProxies = env->GetArrayLength(jProxies);
    std::vector<Proxy> proxies(numProxies);

    for (jsize i = 0; i < numProxies; i++) {
        jobject jProxy = env->GetObjectArrayElement(jProxies, i);
        jclass proxyClass = env->GetObjectClass(jProxy);

        jfieldID websocketUrlField = env->GetFieldID(proxyClass, "websocketUrl", "Ljava/lang/String;");
        jfieldID ethereumAddressField = env->GetFieldID(proxyClass, "ethereumAddress", "Ljava/lang/String;");

        jstring jWebsocketUrl = (jstring)env->GetObjectField(jProxy, websocketUrlField);
        jstring jEthereumAddress = (jstring)env->GetObjectField(jProxy, ethereumAddressField);

        const char* websocketUrl = env->GetStringUTFChars(jWebsocketUrl, 0);
        const char* ethereumAddress = env->GetStringUTFChars(jEthereumAddress, 0);

        proxies[i] = {websocketUrl, ethereumAddress};

        env->ReleaseStringUTFChars(jWebsocketUrl, websocketUrl);
        env->ReleaseStringUTFChars(jEthereumAddress, ethereumAddress);
    }

    Error* errors = nullptr;
    uint64_t numErrors = 0;

    uint64_t result = proxyClientConnect(&errors, &numErrors, static_cast<uint64_t>(clientHandle), proxies.data(), numProxies);

    if (numErrors > 0) {
        /*
        std::string errorMsg = errors[0].message;
        proxyClientFreeErrors(errors, numErrors);
        env->ThrowNew(env->FindClass("com/yourpackage/StreamrProxyClient$StreamrProxyException"), errorMsg.c_str());
         */
        return 0;
    }

    return static_cast<jlong>(result);
}

/*
JNIEXPORT void JNICALL
Java_com_example_locationshare_ProxyClientJNI_proxyClientDisconnect(JNIEnv *env, jclass, jlong clientHandle) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    proxyClientDisconnect(&errors, &numErrors, static_cast<uint64_t>(clientHandle));

    if (numErrors > 0) {

        std::string errorMsg = errors[0].message;
        proxyClientFreeErrors(errors, numErrors);
        env->ThrowNew(env->FindClass("com/yourpackage/StreamrProxyClient$StreamrProxyException"), errorMsg.c_str());

    }
}
*/
JNIEXPORT void JNICALL
Java_com_example_locationshare_ProxyClientJNI_proxyClientPublish(JNIEnv *env, jclass, jlong clientHandle, jstring jContent) {

    const char* content = env->GetStringUTFChars(jContent, 0);
    uint64_t contentLength = strlen(content);
    LOGI("proxyClientPublish clientHandle: %d", clientHandle);

    LOGI("proxyClientPublish string: %s", content);

    Error* errors = nullptr;
    uint64_t numErrors = 0;

    proxyClientPublish(&errors, &numErrors, static_cast<uint64_t>(clientHandle), content, contentLength);

    env->ReleaseStringUTFChars(jContent, content);

    if (numErrors > 0) {
        LOGI("proxyClientPublish error");
        /*
        std::string errorMsg = errors[0].message;
        proxyClientFreeErrors(errors, numErrors);
        env->ThrowNew(env->FindClass("com/yourpackage/StreamrProxyClient$StreamrProxyException"), errorMsg.c_str());
         */
    }
    else {
        LOGI("proxyClientPublish no error");
    }
}

}