// Primary module interface unit of streamr.protorpc. Consumers write
// `import streamr.protorpc;` and get every partition re-exported.
export module streamr.protorpc;

export import :protos;
export import :Errors;
export import :ProtoCallContext;
export import :RpcCommunicator;
export import :RpcCommunicatorClientApi;
export import :RpcCommunicatorServerApi;
export import :ServerRegistry;
export import :StreamPrinter;
