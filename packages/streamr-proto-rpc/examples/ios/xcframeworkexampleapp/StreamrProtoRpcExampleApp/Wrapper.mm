//
//  Wrapper.m
//  StreamrProtoRpcExampleApp
//
//  Created by Santtu Rantanen on 21.8.2024.
//
#include "Wrapper.h"
#include "folly/Format.h"
#include "Hello.hpp"

@implementation Wrapper

- (NSString *)greetingWrapper {
    return [NSString stringWithUTF8String:sayHello().c_str()];
}

@end
