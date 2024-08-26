//
//  ProxyClient.hpp
//  LocationShare
//
//  Created by Santtu Rantanen on 25.8.2024.
//

#ifndef ProxyClient_hpp
#define ProxyClient_hpp

#include <string>
#include "Result.hpp"
#include "PeerDescriptor.hpp"

using ProxyClientHandle = int;

class ProxyClient {
public:
    ProxyClient() { }
    ProxyClientHandle newClient() const;
    Result deleteClient(ProxyClientHandle proxyClientHandle) const;
    Result publish(ProxyClientHandle proxyClientHandle, std::string data) const;
    Result setProxy(ProxyClientHandle proxyClientHandle, PeerDescriptor proxy) const;
};
#endif /* ProxyClient_hpp */
