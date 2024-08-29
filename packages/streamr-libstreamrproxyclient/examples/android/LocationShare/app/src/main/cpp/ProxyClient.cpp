//
// Created by Santtu Rantanen on 28.8.2024.
//

#include "ProxyClient.h"
#include "PeerDescriptor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <jni.h>

using namespace std::chrono_literals;

ProxyClientHandle ProxyClient::newClient() const {
    return 1;
}

Result ProxyClient::deleteClient(ProxyClientHandle proxyClientHandle) const {
    return Result{Result::ResultCode::Ok, ""};
}

Result ProxyClient::publish(ProxyClientHandle proxyClientHandle, std::string data) const {
    std::this_thread::sleep_for(1s);
    return Result{Result::ResultCode::Ok, ""};
}

Result ProxyClient::setProxies(ProxyClientHandle proxyClientHandle, std::vector<PeerDescriptor> proxy) const {
    std::this_thread::sleep_for(1s);
    return Result{Result::ResultCode::Ok, ""};
}

static ProxyClient proxyClient;

// Helper function to create a Result object
jobject createResultObject(JNIEnv* env, const Result& result) {
    jclass resultClass = env->FindClass("com/example/locationshare/ProxyClientJNI$Result");
    jmethodID constructor = env->GetMethodID(resultClass, "<init>", "()V");
    jobject resultObject = env->NewObject(resultClass, constructor);

    jfieldID codeField = env->GetFieldID(resultClass, "code", "I");
    jfieldID messageField = env->GetFieldID(resultClass, "message", "Ljava/lang/String;");

    env->SetIntField(resultObject, codeField, static_cast<jint>(result.code));
    env->SetObjectField(resultObject, messageField, env->NewStringUTF(result.message.c_str()));

    return resultObject;
}


extern "C" {

JNIEXPORT jlong JNICALL
Java_com_example_locationshare_ProxyClientJNI_newClient(JNIEnv *env, jclass) {
    return static_cast<jlong>(proxyClient.newClient());
}

JNIEXPORT jobject JNICALL
Java_com_example_locationshare_ProxyClientJNI_deleteClient(JNIEnv *env, jclass, jlong handle) {
    Result result = proxyClient.deleteClient(static_cast<ProxyClientHandle>(handle));
    return createResultObject(env, result);
}

JNIEXPORT jobject JNICALL
Java_com_example_locationshare_ProxyClientJNI_publish(JNIEnv *env, jclass, jlong handle, jstring jdata) {
    const char *cData = env->GetStringUTFChars(jdata, nullptr);
    std::string data(cData);
    env->ReleaseStringUTFChars(jdata, cData);

    Result result = proxyClient.publish(static_cast<ProxyClientHandle>(handle), data);
    return createResultObject(env, result);
}

JNIEXPORT jobject JNICALL
Java_com_example_locationshare_ProxyClientJNI_setProxies(JNIEnv *env, jclass, jlong handle, jobjectArray jpeerIds, jobjectArray jpeerAddresses) {
    int size = env->GetArrayLength(jpeerIds);
    std::vector<PeerDescriptor> proxies;

    for (int i = 0; i < size; i++) {
        jstring jpeerId = (jstring) env->GetObjectArrayElement(jpeerIds, i);
        jstring jpeerAddress = (jstring) env->GetObjectArrayElement(jpeerAddresses, i);

        const char *cPeerId = env->GetStringUTFChars(jpeerId, nullptr);
        const char *cPeerAddress = env->GetStringUTFChars(jpeerAddress, nullptr);

        proxies.push_back({cPeerId, cPeerAddress});

        env->ReleaseStringUTFChars(jpeerId, cPeerId);
        env->ReleaseStringUTFChars(jpeerAddress, cPeerAddress);
        env->DeleteLocalRef(jpeerId);
        env->DeleteLocalRef(jpeerAddress);
    }

    Result result = proxyClient.setProxies(static_cast<ProxyClientHandle>(handle), proxies);
    return createResultObject(env, result);
}
}