#include <google/protobuf/compiler/plugin.h>
#include "streamr-proto-rpc/PluginCodeGenerator.hpp"

int main(int argc, char** argv) {
    streamr::protorpc::PluginCodeGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
