#include <stdio.h>

#include <google/protobuf/compiler/plugin.h>
#include "../include/protobuf-streamr-plugin/PluginCodeGenerator.hpp"

int main(int argc, char** argv)
{
  PluginCodeGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
