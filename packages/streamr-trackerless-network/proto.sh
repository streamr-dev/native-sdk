PROTOC=../../build/vcpkg_installed/arm64-osx/tools/protobuf/protoc
PLUGIN=../streamr-proto-rpc/build/protobuf-streamr-plugin
mkdir -p ./src/proto
mkdir -p ./src/proto/packages/network/protos

# temporarily copy ../streamr-proto-rpc/protos to protos/packages/streamr-proto-rpc/protos

mkdir -p protos/packages/proto-rpc/protos
mkdir -p protos/packages/dht/protos
mkdir -p protos/packages/network/protos
cp -r ../streamr-proto-rpc/protos/* protos/packages/proto-rpc/protos
cp -r ../streamr-dht/protos/* protos/packages/dht/protos
cp protos/NetworkRpc.proto protos/packages/network/protos
${PROTOC} --plugin=protoc-gen-streamr=${PLUGIN} --streamr_out=./src/proto/packages/network/protos --proto_path=./protos --cpp_out=./src/proto packages/network/protos/NetworkRpc.proto --experimental_allow_proto3_optional
${PROTOC} --plugin=protoc-gen-streamr=${PLUGIN} --streamr_out=./src/proto/packages/dht/protos --proto_path=./protos --cpp_out=./src/proto packages/dht/protos/DhtRpc.proto --experimental_allow_proto3_optional
${PROTOC} --proto_path=./protos --cpp_out=./src/proto packages/proto-rpc/protos/ProtoRpc.proto --experimental_allow_proto3_optional
rm -rf protos/packages
