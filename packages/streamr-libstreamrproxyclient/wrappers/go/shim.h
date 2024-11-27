#ifndef SHIM_H
#define SHIM_H

#include "streamrproxyclient.h"

#ifdef __cplusplus
extern "C" {
#endif

void loadSharedLibrary(const char* libFilePath);
void closeSharedLibrary();

void proxyClientInitLibraryWrapper(void); // NOLINT
void proxyClientCleanupLibraryWrapper(void); // NOLINT
void proxyClientResultDeleteWrapper(const ProxyResult* result);
uint64_t proxyClientNewWrapper(const ProxyResult** result, const char* streamId, const char* topic);
void proxyClientDeleteWrapper(const ProxyResult** result, uint64_t id);
uint64_t proxyClientConnectWrapper(const ProxyResult** result, uint64_t id, const Proxy* proxy, uint64_t proxyId);
void proxyClientDisconnectWrapper(const ProxyResult** result, uint64_t id);
uint64_t proxyClientPublishWrapper(const ProxyResult** result, uint64_t id, const char* content, uint64_t contentLength, const char* ethereumPrivateKey);

#ifdef __cplusplus
}
#endif

#endif // SHIM_H
