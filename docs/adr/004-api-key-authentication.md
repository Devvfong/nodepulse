# ADR 004: Header-Based API Key Authentication (`X-API-Key`)

## Status
Accepted

## Date
2026-10-06

## Context
NodePulse exposes sensitive system information, running processes, and service states over an HTTP interface. An authentication and authorization mechanism is necessary to prevent unauthorized telemetry inspection and host enumeration.

Key operational constraints:
- NodePulse is a standalone host agent; it cannot depend on an external Identity Provider (IdP), OAuth2 server, or LDAP directory to validate incoming requests.
- Low-latency verification (<1ms overhead).
- Prevention of timing side-channel attacks.
- Simple client integration via cURL, Prometheus scrapers, and web frontends.
- Health check exemption for infrastructure liveness probes (`/api/v1/health`).

## Decision
We implement **Header-Based API Key Authentication** using the `X-API-Key` HTTP header.
- Verification is performed in a Drogon `HttpFilter` (`AuthFilter`).
- Comparison is performed using constant-time memory comparison (`constant_time_equals`).
- Secrets are loaded exclusively from secure configuration files or the `NODEPULSE_API_KEY` environment variable.
- The liveness/readiness probe (`GET /api/v1/health`) is explicitly exempt from authentication.

## Consequences

### Positive
- **Stateless & Resilient**: Requires zero network calls or external database lookups during authentication.
- **Timing-Safe**: Constant-time verification eliminates timing side-channel vulnerabilities.
- **Simple Client Adoption**: Supported out-of-the-box by HTTP clients, Prometheus `http_headers`, and scripts.
- **Zero Logging Guarantee**: Secrets are masked and excluded from logs and error payloads.

### Negative / Trade-Offs
- **Single Secret Scope**: All clients sharing the configured key possess identical read privileges (no per-user RBAC).
- **Key Rotation**: Rotating the key requires updating configuration and triggering a service reload (`systemctl reload nodepulse`).

## Alternatives Considered
- **HTTP Basic Authentication**: Standardized, but Base64 encoding in `Authorization: Basic ...` is frequently logged accidentally, and Basic Auth is considered legacy.
- **JSON Web Tokens (JWT) / OAuth2**: Overly complex for a local host agent; requires managing token signing keys, expiration, and token issuer infrastructure.
- **Mutual TLS (mTLS)**: Highly secure, but introduces significant certificate authority (CA) and PKI management operational complexity.

