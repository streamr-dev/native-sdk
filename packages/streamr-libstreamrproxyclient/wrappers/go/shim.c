#include <dlfcn.h>
#include "shim.h"

void (*proxyClientInitLibraryPtr)(void); // NOLINT
void (*proxyClientCleanupLibraryPtr)(void); // NOLINT
void (*proxyClientResultDeletePtr)(const ProxyResult*);
uint64_t (*proxyClientNewPtr)(const ProxyResult**, const char*, const char*);
void (*proxyClientDeletePtr)(const ProxyResult**, uint64_t);
uint64_t (*proxyClientConnectPtr)(const ProxyResult**, uint64_t, const Proxy*, uint64_t);
void (*proxyClientDisconnectPtr)(const ProxyResult**, uint64_t);
uint64_t (*proxyClientPublishPtr)(const ProxyResult**, uint64_t, const char*, uint64_t, const char*);
void* proxyClientLibrary;

void loadSharedLibrary(const char* libFilePath) {
    proxyClientLibrary = dlopen(libFilePath, RTLD_LAZY);
    proxyClientInitLibraryPtr = (void (*)(void))dlsym(proxyClientLibrary, "proxyClientInitLibrary"); // NOLINT
    proxyClientCleanupLibraryPtr = (void (*)(void))dlsym(proxyClientLibrary, "proxyClientCleanupLibrary"); // NOLINT
    proxyClientResultDeletePtr = (void (*)(const ProxyResult*))dlsym(proxyClientLibrary, "proxyClientResultDelete");
    proxyClientNewPtr = (uint64_t (*)(const ProxyResult**, const char*, const char*))dlsym(proxyClientLibrary, "proxyClientNew");
    proxyClientDeletePtr = (void (*)(const ProxyResult**, uint64_t))dlsym(proxyClientLibrary, "proxyClientDelete");
    proxyClientConnectPtr = (uint64_t (*)(const ProxyResult**, uint64_t, const Proxy*, uint64_t))dlsym(proxyClientLibrary, "proxyClientConnect");
    proxyClientDisconnectPtr = (void (*)(const ProxyResult**, uint64_t))dlsym(proxyClientLibrary, "proxyClientDisconnect");
    proxyClientPublishPtr = (uint64_t (*)(const ProxyResult**, uint64_t, const char*, uint64_t, const char*))dlsym(proxyClientLibrary, "proxyClientPublish");
}

void closeSharedLibrary() {
    dlclose(proxyClientLibrary);
}

void proxyClientInitLibraryWrapper(void) { // NOLINT
    proxyClientInitLibraryPtr();
}

void proxyClientCleanupLibraryWrapper(void) { // NOLINT
    proxyClientCleanupLibraryPtr();
}

void proxyClientResultDeleteWrapper(const ProxyResult* result) {
    proxyClientResultDeletePtr(result);
}

uint64_t proxyClientNewWrapper(const ProxyResult** result, const char* streamId, const char* topic) {
    return proxyClientNewPtr(result, streamId, topic);
}

void proxyClientDeleteWrapper(const ProxyResult** result, uint64_t id) {
    proxyClientDeletePtr(result, id);
}

uint64_t proxyClientConnectWrapper(const ProxyResult** result, uint64_t id, const Proxy* proxy, uint64_t proxyId) {
    return proxyClientConnectPtr(result, id, proxy, proxyId);
}

void proxyClientDisconnectWrapper(const ProxyResult** result, uint64_t id) {
    proxyClientDisconnectPtr(result, id);
}

uint64_t proxyClientPublishWrapper(const ProxyResult** result, uint64_t id, const char* content, uint64_t contentLength, const char* ethereumPrivateKey) {
    return proxyClientPublishPtr(result, id, content, contentLength, ethereumPrivateKey);
}
