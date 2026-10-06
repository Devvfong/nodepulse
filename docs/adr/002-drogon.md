# ADR 002: Adoption of Drogon Asynchronous Web Framework

## Status
Accepted

## Date
2026-10-06

## Context
NodePulse requires an HTTP web engine to serve REST endpoints (`/api/v1/*`), stream Server-Sent Events (`/api/v1/events`), and expose Prometheus metrics (`/metrics`). 

The framework must satisfy:
- High I/O throughput with minimal latency (<25ms p95).
- Low resident memory footprint (<35MB idle).
- Non-blocking asynchronous event loop architecture based on epoll on Linux.
- Built-in HTTP middleware filtering (for authentication and rate limiting).
- Support for chunked streaming responses (for SSE).
- Native C++ integration without C ABI wrappers or foreign runtimes.

## Decision
We adopt **Drogon** as the primary asynchronous HTTP web framework for NodePulse.

## Consequences

### Positive
- **High Performance**: Built on Trantor's non-blocking epoll event loop, Drogon consistently ranks among the fastest C++ web frameworks in TechEmpower benchmarks.
- **Filter Middleware**: Drogon provides native `HttpFilter` interception pipelines, ideal for `AuthFilter` and `RateLimitFilter`.
- **Chunked Transfer Encoding**: Drogon's asynchronous response writer seamlessly supports persistent Server-Sent Events connections without thread pinning.
- **Thread Pool Flexibility**: Drogon decouples I/O event loops from task processing, allowing heavy background scans to run without starving network sockets.

### Negative / Trade-Offs
- **Macro Routing System**: Drogon utilizes C++ macros (`METHOD_LIST_BEGIN`, `ADD_METHOD_TO`) for controller routing, requiring careful syntax adherence.
- **Dependency Footprint**: Requires linking Trantor, JSONCpp, OpenSSL, and zlib.

## Alternatives Considered
- **Crow**: Lightweight and header-only friendly, but relies largely on synchronous thread-per-request models or basic ASIO, lacking Drogon's mature filter pipeline and chunked streaming support.
- **Pistache**: Modern C++ REST framework, but community maintenance has been sporadic, with less robust HTTP/1.1 chunked streaming.
- **Boost.Beast**: Highly flexible and low-level, but requires immense boilerplate code to build request routing, middleware filters, and connection lifecycles.

