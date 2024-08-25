//
//  ProxyClient.cpp
//  LocationShare
//
//  Created by Santtu Rantanen on 25.8.2024.
//

#include "ProxyClient.hpp"
#include "PeerDescriptor.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

int ProxyClient::newClient() const {
    return 1;
}

Result ProxyClient::deleteClient(int proxyClientHandle) const {
    return Result{0, ""};
}

Result ProxyClient::publish(int proxyClientHandle, std::string data) const {
    std::this_thread::sleep_for(1s);
    return Result{0, ""};
}

Result ProxyClient::setProxy(int proxyClientHandle, PeerDescriptor proxy) const {
    std::this_thread::sleep_for(10s);
    return Result{0, ""};
}

