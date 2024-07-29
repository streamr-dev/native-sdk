mkdir -p ./src/proto
protoc -I=protos --cpp_out=src/proto protos/ProtoRpc.proto --experimental_allow_proto3_optional

mkdir -p ./test/proto
protoc --plugin=protoc-gen-streamr=./build/protobuf-streamr-plugin --streamr_out=./test/proto --proto_path=test/protos test/protos/*.proto --experimental_allow_proto3_optional

