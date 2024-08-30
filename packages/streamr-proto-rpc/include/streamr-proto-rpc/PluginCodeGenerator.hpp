#ifndef STREAMER_PROTORPC_PLUGIN_CODE_GENERATOR_HPP
#define STREAMER_PROTORPC_PLUGIN_CODE_GENERATOR_HPP

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include "streamr-proto-rpc/StreamPrinter.hpp"

namespace streamr::protorpc {

class PluginCodeGenerator : public google::protobuf::compiler::CodeGenerator {
public:
    PluginCodeGenerator() = default;
    ~PluginCodeGenerator() override = default;

    bool Generate(
        const google::protobuf::FileDescriptor* file,
        const std::string& parameter,
        google::protobuf::compiler::GeneratorContext* generatorContext,
        std::string* error) const override {
        return GenerateHeader(file, parameter, generatorContext, error) &&
            GenerateSource(file, parameter, generatorContext, error);
    }

    uint64_t GetSupportedFeatures() const override { // NOLINT
        // Indicate that this code generator supports proto3 optional fields.
        // (Note: don't release your code generator with this flag set until you
        // have actually added and tested your proto3 support!)
        return FEATURE_PROTO3_OPTIONAL;
    }

private:
    [[nodiscard]] static std::string toUpperCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    [[nodiscard]] static std::string getFilenameWithoutExtension(
        const std::string& filepath) {
        // Find the last directory separator
        size_t lastSlash = filepath.find_last_of("/\\");
        std::string filename = filepath.substr(lastSlash + 1);

        // Find the last dot in the filename
        size_t lastDot = filename.find_last_of('.');
        if (lastDot == std::string::npos) {
            return filename; // No extension found
        }
        return filename.substr(0, lastDot);
    }

    bool GenerateServerHeader( // NOLINT
        const google::protobuf::FileDescriptor* file,
        const std::string& /* parameter */,
        google::protobuf::compiler::GeneratorContext* generatorContext,
        std::string* /* error */) const {
        bool emptyIncluded = false;
        const std::string& protoFilename = file->name();
        const std::string protoFilenameWe =
            getFilenameWithoutExtension(protoFilename);
        const std::string headerFilename = protoFilenameWe + ".server.pb.h";
        const std::string typesFilename = protoFilenameWe + ".pb.h";
        const std::string headerGuard =
            "STREAMR_PROTORPC_" + toUpperCase(protoFilenameWe) + "_SERVER_PB_H";

        std::stringstream headerSs;

        headerSs
            << "// Generated by the protocol buffer streamr pluging. DO NOT EDIT!\n";
        headerSs << "// Generated from protobuf file \"" << protoFilename
                 << "\"\n";
        headerSs << "\n";
        headerSs << "#ifndef " << headerGuard << "\n";
        headerSs << "#define " << headerGuard << "\n\n";
        headerSs << "#include \"" << typesFilename << "\" // NOLINT\n";
        headerSs << "#include <folly/experimental/coro/Task.h>\n";
        headerSs << "#include \"streamr-proto-rpc/ProtoCallContext.hpp\" // NOLINT\n";

        std::stringstream sourceSs;

        if (file->package().empty()) {
            sourceSs << "namespace streamr::protorpc {\n";
        } else {
            sourceSs << "namespace " << file->package() << " {\n";
        }
        headerSs << "using streamr::protorpc::ProtoCallContext;\n\n";
        // for each services
        int numServices = file->service_count();
        for (int i = 0; i < numServices; i++) {
            const google::protobuf::ServiceDescriptor* service =
                file->service(i);
            const std::string serviceFullname = service->full_name();
            const std::string& serviceName = service->name();

            sourceSs << "class " << serviceName << " {\n";
            sourceSs << "public:\n";
            sourceSs << "   virtual ~" << serviceName << "() = default;\n";
            // sourceSs << "    \n";
            // for each methods
            int numMethods = service->method_count();
            for (int j = 0; j < numMethods; j++) {
                const google::protobuf::MethodDescriptor* method =
                    service->method(j);
                const std::string methodFullname = method->full_name();
                const std::string& methodName = method->name();

                const google::protobuf::Descriptor* methodInput =
                    method->input_type();
                const std::string methodInputFullname =
                    methodInput->full_name();
                std::string methodInputName = methodInput->name();

                const google::protobuf::Descriptor* methodOutput =
                    method->output_type();

                if (methodInput->name() == "Empty") {
                    if (!emptyIncluded) {
                        headerSs << "#include <google/protobuf/empty.pb.h>\n";
                        emptyIncluded = true;
                    }
                    methodInputName = "google::protobuf::Empty";
                }

                const std::string methodOutputFullname =
                    methodOutput->full_name();
                std::string methodOutputName = methodOutput->name();

                if (methodOutputName == "Empty") {
                    methodOutputName = "void";
                }

                sourceSs
                    << "   virtual " << methodOutputName + " " << methodName
                    << "(const " << methodInputName
                    << "& request, const ProtoCallContext& callContext) = 0;\n";
            }
            sourceSs << "}; // class " << serviceName << "\n";
        }

        if (file->package().empty()) {
            sourceSs << "}; // namespace streamr::protorpc\n";
        } else {
            sourceSs << "}; // namespace " << file->package() << "\n";
        }

        sourceSs << "\n";
        sourceSs << "#endif // " << headerGuard << "\n";
        // output to header file
        google::protobuf::io::ZeroCopyOutputStream* stream =
            generatorContext->Open(headerFilename);
        StreamPrinter printer(
            stream); // StreamPrinter takes ownership of the Stream
        const std::string headerBuffer = headerSs.str() + "\n";
        const std::string sourceBuffer = sourceSs.str() + "\n";

        printer.print(
            reinterpret_cast<const unsigned char*>(headerBuffer.c_str()),
            std::ssize(headerBuffer));
        printer.print(
            reinterpret_cast<const unsigned char*>(sourceBuffer.c_str()),
            std::ssize(sourceBuffer));
        return true;
    }

    bool GenerateClientHeader( // NOLINT
        const google::protobuf::FileDescriptor* file,
        const std::string& /* parameter */,
        google::protobuf::compiler::GeneratorContext* generatorContext,
        std::string* /* error */) const {
        bool emptyIncluded = false;
        const std::string& protoFilename = file->name();
        const std::string protoFilenameWe =
            getFilenameWithoutExtension(protoFilename);
        const std::string headerFilename = protoFilenameWe + ".client.pb.h";
        const std::string typesFilename = protoFilenameWe + ".pb.h";
        const std::string headerGuard =
            "STREAMR_PROTORPC_" + toUpperCase(protoFilenameWe) + "_CLIENT_PB_H";

        std::stringstream headerSs;

        headerSs
            << "// generated by the protocol buffer streamr pluging. DO NOT EDIT!\n";
        headerSs << "// generated from protobuf file \"" << protoFilename
                 << "\"\n";
        headerSs << "\n";
        headerSs << "#ifndef " << headerGuard << "\n";
        headerSs << "#define " << headerGuard << "\n\n";
        headerSs << "#include <folly/experimental/coro/Task.h>\n";
        headerSs << "#include \"" << typesFilename << "\" // NOLINT\n";
        headerSs << "#include \"streamr-proto-rpc/ProtoCallContext.hpp\"\n";
        headerSs << "#include \"streamr-proto-rpc/RpcCommunicator.hpp\"\n";

        headerSs << "\n";

        std::stringstream sourceSs;

        if (file->package().empty()) {
            sourceSs << "namespace streamr::protorpc {\n";
        } else {
            sourceSs << "namespace " << file->package() << " {\n";
        }

        // for each services
        int numServices = file->service_count();
        for (int i = 0; i < numServices; i++) {
            const google::protobuf::ServiceDescriptor* service =
                file->service(i);
            const std::string serviceFullname = service->full_name();
            const std::string& serviceName = service->name();
            sourceSs << "class " << serviceName << "Client"
                     << " {\n";
            sourceSs << "private:\n";
            sourceSs << "RpcCommunicator& communicator;\n";
            sourceSs << "public:\n";
            sourceSs
                << "    " << serviceName
                << "Client(RpcCommunicator& communicator) : communicator(communicator) {}\n";
            // sourceSs << "    \n";
            // for each methods
            int numMethods = service->method_count();
            for (int j = 0; j < numMethods; j++) {
                const google::protobuf::MethodDescriptor* method =
                    service->method(j);
                const std::string methodFullname = method->full_name();
                const std::string& methodName = method->name();

                const google::protobuf::Descriptor* methodInput =
                    method->input_type();
                const std::string methodInputFullname =
                    methodInput->full_name();
                std::string methodInputName = methodInput->name();

                const google::protobuf::Descriptor* methodOutput =
                    method->output_type();

                if (methodInput->name() == "Empty") {
                    if (!emptyIncluded) {
                        headerSs << "#include <google/protobuf/empty.pb.h>\n";
                        emptyIncluded = true;
                    }
                    methodInputName = "google::protobuf::Empty";
                }

                const std::string methodOutputFullname =
                    methodOutput->full_name();
                std::string methodOutputName = methodOutput->name();

                if (methodOutputName == "Empty") {
                    methodOutputName = "void";
                }

                sourceSs
                    << "    folly::coro::Task<" + methodOutputName + "> "
                    << methodName << "(const " << methodInputName
                    << "& request, const ProtoCallContext& callContext) {\n";
                sourceSs << "        return communicator.";
                if (methodOutputName == "void") {
                    sourceSs << "notify<" << methodInputName << ">";
                } else {
                    sourceSs << "request<" << methodOutputName << ", "
                             << methodInputName << ">";
                }
                sourceSs << "(\"" << methodName
                         << "\", request, callContext);\n";
                sourceSs << "    }\n";
            }
            sourceSs << "}; // class " << serviceName << "Client\n";
        }

        if (file->package().empty()) {
            sourceSs << "}; // namespace streamr::protorpc\n";
        } else {
            sourceSs << "}; // namespace " << file->package() << "\n";
        }

        sourceSs << "\n";
        sourceSs << "#endif // " << headerGuard << "\n";
        // output to header file
        google::protobuf::io::ZeroCopyOutputStream* stream =
            generatorContext->Open(headerFilename);
        StreamPrinter printer(
            stream); // StreamPrinter takes ownership of the Stream
        const std::string headerBuffer = headerSs.str() + "\n";
        const std::string sourceBuffer = sourceSs.str() + "\n";

        printer.print(
            reinterpret_cast<const unsigned char*>(headerBuffer.c_str()),
            std::ssize(headerBuffer));
        printer.print(
            reinterpret_cast<const unsigned char*>(sourceBuffer.c_str()),
            std::ssize(sourceBuffer));
        return true;
    }

    bool GenerateHeader( // NOLINT
        const google::protobuf::FileDescriptor* file,
        const std::string& parameter,
        google::protobuf::compiler::GeneratorContext* generatorContext,
        std::string* error) const {
        std::cerr << "GenerateHeader()"
                  << "\n";
        return GenerateServerHeader(file, parameter, generatorContext, error) &&
            GenerateClientHeader(file, parameter, generatorContext, error);
    }

    bool GenerateSource( // NOLINT
        const google::protobuf::FileDescriptor* /* file */,
        const std::string& /* parameter */,
        google::protobuf::compiler::GeneratorContext* /* generatorContext */,
        std::string* /* error */) const {
        std::cerr << "GenerateSource()"
                  << "\n";
        return true;
    }
};
}; // namespace streamr::protorpc
#endif // PROTOBUF_STREAMR_PLUGIN_PLUGIN_CODE_GENERATOR_HPP