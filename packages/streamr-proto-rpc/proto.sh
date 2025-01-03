PROTOC=../../build/vcpkg_installed/arm64-osx/tools/protobuf/protoc
PLUGIN=./build/protobuf-streamr-plugin

# temporarily copy protos/*.proto to protos/packages/streamr-proto-rpc/protos

mkdir -p protos/packages/proto-rpc/protos
cp protos/*.proto protos/packages/proto-rpc/protos

mkdir -p ./src/proto
${PROTOC} -I=protos --cpp_out=src/proto protos/packages/proto-rpc/protos/ProtoRpc.proto --experimental_allow_proto3_optional
rm -rf protos/packages

mkdir -p ./test/proto
${PROTOC} -I=test/protos --cpp_out=./test/proto test/protos/*.proto --experimental_allow_proto3_optional
${PROTOC} --plugin=protoc-gen-streamr=${PLUGIN} --streamr_out=./test/proto --proto_path=test/protos test/protos/*.proto --experimental_allow_proto3_optional

mkdir -p ./examples/hello/proto
${PROTOC} -I=examples/hello --cpp_out=./examples/hello/proto examples/hello/*.proto --experimental_allow_proto3_optional
${PROTOC} --plugin=protoc-gen-streamr=${PLUGIN} --streamr_out=./examples/hello/proto --proto_path=examples/hello examples/hello/*.proto --experimental_allow_proto3_optional

mkdir -p ./examples/routed-hello/proto
${PROTOC} -I=examples/routed-hello --cpp_out=./examples/routed-hello/proto examples/routed-hello/*.proto --experimental_allow_proto3_optional
${PROTOC} --plugin=protoc-gen-streamr=${PLUGIN} --streamr_out=./examples/routed-hello/proto --proto_path=examples/routed-hello examples/routed-hello/*.proto --experimental_allow_proto3_optional
