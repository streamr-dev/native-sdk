// NOLINTBEGIN

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "streamrproxyclient.h"

int main() {

    // This is a widely-used test account
    const char* ownEthereumAddress = 
        "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5";
    const char* ethereumPrivateKey = 
        "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";

    const char* proxyUrl = "ws://95.216.15.80:44211";
    const char* proxyServerEthereumAddress = "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f";
    const char* streamPartId = "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0";

    const ProxyResult* proxyResult = NULL;

    proxyClientInitLibrary();
    
    uint64_t clientHandle = 
        proxyClientNew(&proxyResult, ownEthereumAddress, streamPartId);

    assert(proxyResult->numErrors == 0);
    proxyClientResultDelete(proxyResult);

    Proxy proxy;
    proxy.websocketUrl = proxyUrl;
    proxy.ethereumAddress = proxyServerEthereumAddress;

    proxyClientConnect(&proxyResult, clientHandle, &proxy, 1);

    assert(proxyResult->numErrors == 0);
    proxyClientResultDelete(proxyResult);

    const char* message = "Hello from libstreamrproxyclient!";

    printf("Publishing message\n");
    uint64_t numProxiesPublishedTo = proxyClientPublish(
        &proxyResult,
        clientHandle,
        message,
        strlen(message),
        ethereumPrivateKey);

    assert(proxyResult->numErrors == 0);
    proxyClientResultDelete(proxyResult);

    printf("%s published message \"%s\" to %llu proxies\n",
           ownEthereumAddress, message, numProxiesPublishedTo);

    proxyClientDelete(&proxyResult, clientHandle);

    assert(proxyResult->numErrors == 0);
    proxyClientResultDelete(proxyResult);

    proxyClientCleanupLibrary();
    return 0;
}
// NOLINTEND