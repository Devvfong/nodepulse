# NodePulse — Consolidated Project Specification

## 1. Executive Summary
NodePulse is a lightweight, high-performance Linux host monitoring and management agent written in **C++20** using the **Drogon** asynchronous web framework. It exposes a secure HTTP REST API (`/api/v1`), real-time Server-Sent Events (`/api/v1/events`), and Prometheus exposition (`/metrics`). The agent extracts telemetry directly from Linux virtual filesystems (`/proc`, `/sys`) and POSIX system calls with minimal overhead and zero reliance on shell execution.

---

## 2. Specification Quick Reference Matrix

| Dimension | Specification Link | Stable IDs | Count |
|---|---|---|---|
| **Product & Vision** | [PRODUCT.md](file:///home/devqii/workspace/nodepulse/docs/product/PRODUCT.md) | Vision, Personas, Scope | N/A |
| **Functional Requirements** | [REQUIREMENTS.md](file:///home/devqii/workspace/nodepulse/docs/product/REQUIREMENTS.md) | `FR-001` – `FR-020` | 20 |
| **Non-Functional Requirements** | [NON_FUNCTIONAL_REQUIREMENTS.md](file:///home/devqii/workspace/nodepulse/docs/product/NON_FUNCTIONAL_REQUIREMENTS.md) | `NFR-001` – `NFR-010` | 10 |
| **Business & Execution Rules** | [BUSINESS_RULES.md](file:///home/devqii/workspace/nodepulse/docs/domain/BUSINESS_RULES.md) | `BR-001` – `BR-012` | 12 |
| **Use Cases** | [USE_CASES.md](file:///home/devqii/workspace/nodepulse/docs/product/USE_CASES.md) | `UC-001` – `UC-015` | 15 |
| **Architectural Decisions** | [DECISIONS.md](file:///home/devqii/workspace/nodepulse/DECISIONS.md) & [docs/adr/](file:///home/devqii/workspace/nodepulse/docs/adr/) | `DEC-001` – `DEC-013`, `ADR-001` – `ADR-006` | 6 ADRs, 13 Decisions |
| **API Endpoints** | [API.md](file:///home/devqii/workspace/nodepulse/docs/api/API.md) | 14 routes under `/api/v1` and `/metrics` | 14 |
| **Standard Error Codes** | [ERRORS.md](file:///home/devqii/workspace/nodepulse/docs/api/ERRORS.md) | 9 standard error types | 9 |
| **Implementation Plan** | [IMPLEMENTATION_PLAN.md](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md) | Phase 1 – Phase 16 | 16 |

---

## 3. Core Architectural Constraints
1. **Layer Isolation**: Controllers must never parse `/proc` or `/sys` files directly. Collectors must never depend on Drogon, HTTP headers, or JSON serializations. Services coordinate collectors and domain structures.
2. **Type Safety Across Boundaries**: Inter-layer communication uses strongly typed C++ domain structures (`CpuMetrics`, `MemoryMetrics`, etc.), confining JSON manipulation exclusively to controllers.
3. **Security Invariants**:
   - `X-API-Key` header authentication enforced on all protected endpoints; `GET /api/v1/health` is exempt for infrastructure liveness probes; `GET /metrics` is secure-by-default and requires authentication unless explicitly disabled via configuration.
   - Secrets are loaded from configuration/environment, never hardcoded, and never logged.
   - Access to `/var/run/docker.sock` is treated as root-equivalent capability and strictly documented as such.
   - Running as a `--privileged` container is prohibited.
4. **Resilience & Graceful Degradation**: If an optional subsystem (such as Docker or PostgreSQL) is unavailable, the agent responds with standard error codes (`DOCKER_UNAVAILABLE`, `COLLECTOR_FAILURE`) without terminating the process.
5. **Worker Pool Offload Strategy**: Heavy Linux subsystem traversals (such as full process table inspection across `/proc/[0-9]+`) and external daemon socket queries are offloaded to an asynchronous worker thread pool as an intentional architectural decision ([DEC-013](file:///home/devqii/workspace/nodepulse/DECISIONS.md#dec-013-worker-thread-pool-offloading-for-heavy-linux-subsystem-operations)) to safeguard Drogon event loop responsiveness.

