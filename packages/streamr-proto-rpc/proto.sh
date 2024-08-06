PROTOC=build/vcpkg_installed/arm64-osx/tools/protobuf/protoc
mkdir -p ./src/proto
${PROTOC} -I=protos --cpp_out=src/proto protos/ProtoRpc.proto --experimental_allow_proto3_optional

mkdir -p ./test/proto
${PROTOC} -I=test/protos --cpp_out=./test/proto test/protos/*.proto --experimental_allow_proto3_optional
${PROTOC} --plugin=protoc-gen-streamr=./build/protobuf-streamr-plugin --streamr_out=./test/proto --proto_path=test/protos test/protos/*.proto --experimental_allow_proto3_optional
