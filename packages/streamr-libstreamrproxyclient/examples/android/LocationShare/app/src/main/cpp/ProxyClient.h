//
// Created by Santtu Rantanen on 28.8.2024.
//

#ifndef LOCATIONSHARE_PROXYCLIENT_H
#define LOCATIONSHARE_PROXYCLIENT_H

#include <string>
#include "Result.h"
#include "PeerDescriptor.h"

using ProxyClientHandle = int;

class ProxyClient {
public:
    ProxyClient() { }
    ProxyClientHandle newClient() const;
    Result deleteClient(ProxyClientHandle proxyClientHandle) const;
    Result publish(ProxyClientHandle proxyClientHandle, std::string data) const;
    Result setProxies(ProxyClientHandle proxyClientHandle, std::vector<PeerDescriptor> proxy) const;
};


#endif //LOCATIONSHARE_PROXYCLIENT_H
