//
//  ProxyClient.hpp
//  LocationShare
//
//  Created by Santtu Rantanen on 25.8.2024.
//

#ifndef ProxyClient_hpp
#define ProxyClient_hpp

//#define GLOG_USE_GLOG_EXPORT

#include <string>
#include "Result.hpp"
#include "streamrproxyclient.h"

class ProxyClient {
private:
    
public:
    ProxyClient() { }
    uint64_t newClient(std::string ownEthereumAddress, std::string streamPartId) const;
    Result deleteClient(uint64_t proxyClientHandle) const;
    Result publish(uint64_t proxyClientHandle, std::string data, std::string ethereumPrivateKey) const;
    uint64_t connect(uint64_t proxyClientHandle, std::string websocketUrl,
                     std::string ethereumAddress) const;
};
#endif /* ProxyClient_hpp */
