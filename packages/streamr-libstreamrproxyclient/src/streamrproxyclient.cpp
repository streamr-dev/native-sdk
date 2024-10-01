#include "streamrproxyclient.h"
#include "LibProxyClientApi.hpp"
#include <folly/Singleton.h>

using streamr::libstreamrproxyclient::LibProxyClientApi;

const char* testRpc() {
    return "Hajotkaa siihen";
}

static void initFolly() {
    /*
    static std::vector<std::string> arguments = {"streamrproxyclient", "something", "somethingelse"};

    static std::vector<char*> argv;
    for (const auto& arg : arguments) {
        argv.push_back((char*)arg.data());
    }
    argv.push_back(nullptr);

    static auto argvpointer = argv.data();
    static int argcsize = static_cast<int>(argv.size() - 1);
    
    
    //gflags::ParseCommandLineFlags(&argcsize, &argvpointer, false);
    
    int argc = 1;
    const auto arg0 = "dummy";
    char* argv0 = const_cast<char*>(arg0);
    char** argv = &argv0;
    
    folly::InitOptions options;
    options.use_gflags = false;
    static folly::Init init(&argc, &argv, options);
    return init;
    */
    folly::SingletonVault::singleton()->registrationComplete();
}

void initialize() {
    //getFollyInit();
}

static LibProxyClientApi& getProxyClientApi() { // NOLINT
    static LibProxyClientApi proxyClientApi; // NOLINT
    return proxyClientApi;
}

uint64_t proxyClientNew(
    Error** errors,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId) {
    initFolly(); // this can be safely called multiple times
    return getProxyClientApi().proxyClientNew(
        errors, numErrors, ownEthereumAddress, streamPartId);
}

void proxyClientDelete(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle) {
    getProxyClientApi().proxyClientDelete(errors, numErrors, clientHandle);
}

uint64_t proxyClientConnect(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies) {
    return getProxyClientApi().proxyClientConnect(
        errors, numErrors, clientHandle, proxies, numProxies);
}

void proxyClientPublish(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength) {
    getProxyClientApi().proxyClientPublish(
        errors, numErrors, clientHandle, content, contentLength);
}
