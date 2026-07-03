// Primary module interface unit of streamr.eventemitter. Consumers write
// `import streamr.eventemitter;` and get every partition re-exported.
// (Façade migration, MODERNIZATION.md Part 2: one partition per public
// header; headers remain the source of truth until consolidation.)
export module streamr.eventemitter;

export import :EventEmitter;
