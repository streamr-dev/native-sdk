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

class ProxyClient {
public:
    ProxyClient() { }
    int newClient() const;
    Result deleteClient(int proxyClientHandle) const;
    Result publish(int proxyClientHandle, std::string data) const;
    Result setProxy(int proxyClientHandle, PeerDescriptor proxy) const;
};
#endif /* ProxyClient_hpp */
