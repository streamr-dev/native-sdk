# Use the protoc that matches the installed libprotobuf; triplet detected
# from the monorepo build dir (override with PROTOC=... if needed).
PROTOC=${PROTOC:-$(ls ../../build/vcpkg_installed/*/tools/protobuf/protoc 2>/dev/null | head -1)}
#PLUGIN=../../build/packages/streamr-proto-rpc/protobuf-streamr-plugin
PLUGIN=../streamr-proto-rpc/build/protobuf-streamr-plugin
mkdir -p ./src/proto
mkdir -p ./src/proto/packages/dht/protos

# temporarily copy ../streamr-proto-rpc/protos to protos/packages/streamr-proto-rpc/protos

mkdir -p protos/packages/proto-rpc/protos
mkdir -p protos/packages/dht/protos
cp -r ../streamr-proto-rpc/protos/* protos/packages/proto-rpc/protos
cp protos/DhtRpc.proto protos/packages/dht/protos
mkdir -p ./modules/gen
# The plugin emits C++ module units (Phase 2.6): module_prefix names the
# module family; the units land under modules/gen so the module FILE_SET
# glob picks them up.
${PROTOC} --plugin=protoc-gen-streamr=${PLUGIN} "--streamr_out=module_prefix=streamr.dht:./modules/gen" --proto_path=./protos --cpp_out=./src/proto packages/dht/protos/DhtRpc.proto
${PROTOC} --proto_path=./protos --cpp_out=./src/proto packages/proto-rpc/protos/ProtoRpc.proto
rm -rf protos/packages

#mkdir -p ./test/proto
#${PROTOC} -I=test/protos --cpp_out=./test/proto test/protos/*.proto
#${PROTOC} --plugin=protoc-gen-streamr=./build/protobuf-streamr-plugin --streamr_out=./test/proto --proto_path=test/protos test/protos/*.proto

#mkdir -p ./examples/hello/proto
#${PROTOC} -I=examples/hello --cpp_out=./examples/hello/proto examples/hello/*.proto
#${PROTOC} --plugin=protoc-gen-streamr=./build/protobuf-streamr-plugin --streamr_out=./examples/hello/proto --proto_path=examples/hello examples/hello/*.proto

#mkdir -p ./examples/routed-hello/proto
#${PROTOC} -I=examples/routed-hello --cpp_out=./examples/routed-hello/proto examples/routed-hello/*.proto
#${PROTOC} --plugin=protoc-gen-streamr=./build/protobuf-streamr-plugin --streamr_out=./examples/routed-hello/proto --proto_path=examples/routed-hello examples/routed-hello/*.proto
