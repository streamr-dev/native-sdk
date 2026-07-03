// Primary module interface unit of streamr.json. Consumers write
// `import streamr.json;` and get every partition re-exported.
// (Façade migration, MODERNIZATION.md Part 2: one partition per public
// header; headers remain the source of truth until consolidation.)
export module streamr.json;

export import :jsonConcepts;
export import :toJson;
export import :toString;
