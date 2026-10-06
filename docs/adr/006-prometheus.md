# ADR 006: Prometheus Exposition Format for Metrics Scraping

## Status
Accepted

## Date
2026-10-06

## Context
NodePulse must integrate cleanly into modern cloud-native observability stacks. The monitoring ecosystem (Prometheus, VictoriaMetrics, Grafana Mimir, Datadog Agent) widely relies on pull-based HTTP scraping using the Prometheus text exposition format.

Requirements:
- Standard Prometheus text format (`text/plain; version=0.0.4; charset=utf-8`).
- Low allocation overhead during metric generation.
- Clear separation between host resource metrics (CPU, RAM, disk, network) and NodePulse internal health metrics (request count, latency histograms, collector execution times).

## Decision
We expose `/metrics` using standard **Prometheus text exposition format**, implemented either via `prometheus-cpp` or a lightweight, dedicated, zero-copy metric serializer in Phase 13.
The `/metrics` endpoint is **secure-by-default**, requiring a valid `X-API-Key` header unless explicitly disabled by an administrator via `prometheus.require_auth: false` in configuration.

## Consequences

### Positive
- **Instant Ecosystem Compatibility**: Native out-of-the-box compatibility with standard Prometheus server configurations, Grafana Dashboards, and alerting pipelines.
- **Human-Readable Diagnostics**: The text-based format allows operators to inspect raw metrics easily using `curl http://host/metrics`.
- **Pull-Based Decoupling**: The agent does not need to know central monitoring server IP addresses, ports, or credentials; scrapers poll the agent at their own configured intervals.

### Negative / Trade-Offs
- **Network Ingress Dependency**: Scrapers must be able to reach the agent's port, requiring appropriate firewall rules or internal VPC routing.
- **Serialization Overhead**: Generating human-readable text on high-frequency scrapes consumes more CPU than binary protocols (mitigated by caching and zero-copy string formatting).

## Alternatives Considered
- **StatsD (UDP Push)**: Fire-and-forget UDP pushes, but lacks standardized metric metadata, labels/tags, and type safety.
- **OpenTelemetry OTLP Push**: Modern industry standard for pushing traces and metrics, but introduces massive SDK dependencies and requires a running OTel Collector daemon.

