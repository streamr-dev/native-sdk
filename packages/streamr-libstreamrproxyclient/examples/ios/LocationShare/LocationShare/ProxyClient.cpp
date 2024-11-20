//
//  ProxyClient.cpp
//  LocationShare
//
//  Created by Santtu Rantanen on 25.8.2024.
//

#include "ProxyClient.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/StreamPartID.hpp"
#include "streamrproxyclient.h"

using streamr::utils::toEthereumAddress;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;

using namespace std::chrono_literals;

uint64_t ProxyClient::newClient(std::string ownEthereumAddress, std::string streamPartId) const {
    Error* errors = nullptr;
    uint64_t numErrors = 0;
    uint64_t clientHandle = proxyClientNew(&errors, &numErrors, ownEthereumAddress.c_str(), streamPartId.c_str());

    if (numErrors > 0) {
        return 0;
    }
    return clientHandle;
}

Result ProxyClient::deleteClient(uint64_t proxyClientHandle) const {
    return Result{Result::ResultCode::Ok, ""};
}

Result ProxyClient::publish(uint64_t proxyClientHandle, std::string data, std::string ethereumPrivateKey) const {
    Error* errors = nullptr;
    uint64_t numErrors = 0;
    proxyClientPublish(&errors, &numErrors, proxyClientHandle, data.c_str(), data.length(), ethereumPrivateKey.c_str());
    std::this_thread::sleep_for(1s);
    return Result{Result::ResultCode::Ok, ""};
}

uint64_t ProxyClient::connect(uint64_t proxyClientHandle, std::string websocketUrl,
                              std::string ethereumAddress) const {
    Error* errors = nullptr;
    uint64_t numErrors = 0;
    auto proxy = Proxy(websocketUrl.c_str(), ethereumAddress.c_str());
    uint64_t result = proxyClientConnect(&errors, &numErrors, proxyClientHandle, &proxy, 1);
    if (numErrors > 0) {
        return 0;
    }
    return result;
   
}
