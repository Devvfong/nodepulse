# NodePulse Documentation Index

Welcome to the authoritative technical documentation for **NodePulse**, a high-performance Linux server monitoring and management agent written in C++20 using Drogon.

---

## 1. Project Governance & Execution

- [README.md](file:///home/devqii/workspace/nodepulse/README.md) — High-level project summary, quick links, and goals.
- [AGENTS.md](file:///home/devqii/workspace/nodepulse/AGENTS.md) — Mandatory execution policy and safety rules for AI coding agents and human contributors.
- [CURRENT_STATE.md](file:///home/devqii/workspace/nodepulse/CURRENT_STATE.md) — Current implementation phase status, artifact inventory, and active approvals.
- [DECISIONS.md](file:///home/devqii/workspace/nodepulse/DECISIONS.md) — Architectural decision log linking all design choices and ADRs.
- [IMPLEMENTATION_PLAN.md](file:///home/devqii/workspace/nodepulse/IMPLEMENTATION_PLAN.md) — Rigorous 16-phase implementation roadmap with scopes, tests, and exit gates.
- [PROJECT_SPEC.md](file:///home/devqii/workspace/nodepulse/docs/PROJECT_SPEC.md) — Consolidated executive specification and requirements cross-reference.

---

## 2. Product Documentation (`docs/product/`)

- [PRODUCT.md](file:///home/devqii/workspace/nodepulse/docs/product/PRODUCT.md) — Product vision, mission, target personas, and scope boundaries.
- [REQUIREMENTS.md](file:///home/devqii/workspace/nodepulse/docs/product/REQUIREMENTS.md) — Complete functional requirements catalog with stable IDs (`FR-001` to `FR-020`).
- [USE_CASES.md](file:///home/devqii/workspace/nodepulse/docs/product/USE_CASES.md) — Comprehensive user and system interaction use cases (`UC-001` to `UC-015`).
- [NON_FUNCTIONAL_REQUIREMENTS.md](file:///home/devqii/workspace/nodepulse/docs/product/NON_FUNCTIONAL_REQUIREMENTS.md) — Quality attributes, latency, resource footprint, security, and portability goals (`NFR-001` to `NFR-010`).

---

## 3. Domain Documentation (`docs/domain/`)

- [DOMAIN_MODEL.md](file:///home/devqii/workspace/nodepulse/docs/domain/DOMAIN_MODEL.md) — Domain entity definitions, aggregates, and value objects.
- [BUSINESS_RULES.md](file:///home/devqii/workspace/nodepulse/docs/domain/BUSINESS_RULES.md) — Business constraints, validation logic, and execution rules (`BR-001` to `BR-012`).
- [DATA_MODEL.md](file:///home/devqii/workspace/nodepulse/docs/domain/DATA_MODEL.md) — In-memory C++ domain structures and relational database schemas.

---

## 4. Architecture Documentation (`docs/architecture/`)

- [ARCHITECTURE.md](file:///home/devqii/workspace/nodepulse/docs/architecture/ARCHITECTURE.md) — High-level architecture, layer responsibilities, and boundary rules.
- [COMPONENTS.md](file:///home/devqii/workspace/nodepulse/docs/architecture/COMPONENTS.md) — Detailed subsystem breakdowns for controllers, collectors, services, and middlewares.
- [DATA_FLOW.md](file:///home/devqii/workspace/nodepulse/docs/architecture/DATA_FLOW.md) — End-to-end data flow diagrams for REST queries, SSE streaming, and Prometheus scraping.
- [OBSERVABILITY.md](file:///home/devqii/workspace/nodepulse/docs/architecture/OBSERVABILITY.md) — Logging standards, self-telemetry, metrics exposition, and diagnostic tracing.
- [SECURITY.md](file:///home/devqii/workspace/nodepulse/docs/architecture/SECURITY.md) — Threat modeling, API key handling, Docker socket risk analysis, and sandboxing.

---

## 5. API Documentation (`docs/api/`)

- [API.md](file:///home/devqii/workspace/nodepulse/docs/api/API.md) — Complete REST and SSE API contract specification for `/api/v1` and `/metrics`.
- [AUTHENTICATION.md](file:///home/devqii/workspace/nodepulse/docs/api/AUTHENTICATION.md) — `X-API-Key` security model, header verification, and constant-time checking.
- [ERRORS.md](file:///home/devqii/workspace/nodepulse/docs/api/ERRORS.md) — Standardized JSON error response envelope and complete error code catalog.

---

## 6. Engineering Documentation (`docs/engineering/`)

- [BUILD.md](file:///home/devqii/workspace/nodepulse/docs/engineering/BUILD.md) — Toolchain prerequisites, CMake build targets, options, and dependency management.
- [CODING_STANDARDS.md](file:///home/devqii/workspace/nodepulse/docs/engineering/CODING_STANDARDS.md) — C++20 conventions, formatting rules, memory management, and clang-tidy checks.
- [CONTRIBUTING.md](file:///home/devqii/workspace/nodepulse/docs/engineering/CONTRIBUTING.md) — Contributor onboarding, development lifecycle, and pull request checklist.
- [GIT_WORKFLOW.md](file:///home/devqii/workspace/nodepulse/docs/engineering/GIT_WORKFLOW.md) — Branching strategies, conventional commit standards, and release tags.
- [TESTING.md](file:///home/devqii/workspace/nodepulse/docs/engineering/TESTING.md) — Unit testing, `/proc` fixture testing, integration testing, sanitizers, and CI pipelines.

---

## 7. Operations Documentation (`docs/operations/`)

- [CONFIGURATION.md](file:///home/devqii/workspace/nodepulse/docs/operations/CONFIGURATION.md) — JSON configuration options, environment variable overrides, and validator.
- [DEPLOYMENT.md](file:///home/devqii/workspace/nodepulse/docs/operations/DEPLOYMENT.md) — Bare-metal, virtual machine, and non-privileged Docker container deployment runbook.
- [MONITORING.md](file:///home/devqii/workspace/nodepulse/docs/operations/MONITORING.md) — Prometheus scraping guide, alert rules, and Grafana dashboard templates.
- [SYSTEMD.md](file:///home/devqii/workspace/nodepulse/docs/operations/SYSTEMD.md) — Hardened systemd service unit file, sandbox directives, and daemon management.
- [TROUBLESHOOTING.md](file:///home/devqii/workspace/nodepulse/docs/operations/TROUBLESHOOTING.md) — Failure scenarios, diagnostic procedures, log interpretation, and FAQ.

---

## 8. Architectural Decision Records (`docs/adr/`)

- [001-cpp20.md](file:///home/devqii/workspace/nodepulse/docs/adr/001-cpp20.md) — Standardize on ISO C++20.
- [002-drogon.md](file:///home/devqii/workspace/nodepulse/docs/adr/002-drogon.md) — Adopt Drogon Asynchronous Web Framework.
- [003-cmake.md](file:///home/devqii/workspace/nodepulse/docs/adr/003-cmake.md) — Modern CMake Build System (>= 3.22).
- [004-api-key-authentication.md](file:///home/devqii/workspace/nodepulse/docs/adr/004-api-key-authentication.md) — Header-Based `X-API-Key` Authentication.
- [005-sse.md](file:///home/devqii/workspace/nodepulse/docs/adr/005-sse.md) — Server-Sent Events (SSE) for Real-Time Telemetry.
- [006-prometheus.md](file:///home/devqii/workspace/nodepulse/docs/adr/006-prometheus.md) — Prometheus Exposition Format (`/metrics`).

