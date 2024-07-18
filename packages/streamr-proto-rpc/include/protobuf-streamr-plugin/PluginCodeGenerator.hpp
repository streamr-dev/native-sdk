#ifndef PROTOBUF_STREAMR_PLUGIN_HPP
#define PROTOBUF_STREAMR_PLUGIN_HPP

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <fstream>
#include <iostream>

class PluginCodeGenerator : public google::protobuf::compiler::CodeGenerator {
public:
    PluginCodeGenerator() = default;
    virtual ~PluginCodeGenerator() = default;

    bool Generate(
        const google::protobuf::FileDescriptor* file,
        const std::string& parameter,
        google::protobuf::compiler::GeneratorContext* generator_context,
        std::string* error) const {
        
        std::ofstream outFile("tmp.txt");
        if (outFile.is_open()) {
            outFile << "Hello";
            outFile.close();
            std::cout << "Successfully wrote to tmp.txt" << std::endl;
        } else {
            std::cerr << "Unable to open file" << std::endl;
        }

        bool success =
            GenerateHeader(file, parameter, generator_context, error);
        if (!success) {
            return false;
        }

        if (error) {
            std::string& e = (*error);
            // TODO error handling
        }

        return true;
    }

private:
    bool GenerateHeader(
        const google::protobuf::FileDescriptor* file,
        const std::string& parameter,
        google::protobuf::compiler::GeneratorContext* generator_context,
        std::string* error) const {
        return true;
    }
    bool GenerateSource(
        const google::protobuf::FileDescriptor* file,
        const std::string& parameter,
        google::protobuf::compiler::GeneratorContext* generator_context,
        std::string* error) const {
        return true;
    }
};

#endif // PROTOBUF_STREAMR_PLUGIN_HPP