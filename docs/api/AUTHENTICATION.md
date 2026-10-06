# NodePulse — Authentication Guide

This document details the header-based API key authentication model used to secure the NodePulse agent API.

---

## 1. Authentication Scheme Overview

NodePulse enforces **Header-Based API Key Authentication** across all monitoring and management endpoints.

Clients authenticate requests by including the `X-API-Key` header:

```http
GET /api/v1/cpu HTTP/1.1
Host: 127.0.0.1:8080
X-API-Key: np_live_8f3a92b0c1e84d2f83a5e1029c7b419d
```

---

## 2. Authentication Rules & Exemptions

1. **Mandatory Enforcement**:
   All endpoints under `/api/v1/` require a valid `X-API-Key` header, except `/api/v1/health`.
2. **Health Check Exemption (BR-001)**:
   `GET /api/v1/health` does not require authentication. This ensures local systemd health checks, Kubernetes liveness probes, and upstream load balancers can verify agent availability without credential provisioning.
3. **Prometheus Scraping (`/metrics`) — Secure by Default**:
   `GET /metrics` is authenticated by default and strictly requires a valid `X-API-Key` header (`prometheus.require_auth: true`). It may be accessed without authentication ONLY when explicitly configured by setting `prometheus.require_auth: false` in `config.json` (e.g. for scrapers operating across a physically or logically isolated management VPC). If `prometheus.require_auth` is true (the default) or omitted, unauthenticated scrape requests are rejected with HTTP 401 `UNAUTHORIZED`.

---

## 3. Secret Management & Storage

- **Configuration File**:
  The primary key is specified in `config/config.json`:
  ```json
  {
    "security": {
      "api_key": "YOUR_SECURE_API_KEY",
      "constant_time_comparison": true
    }
  }
  ```
- **Environment Variable Override**:
  In containerized or automated environments, the key can be supplied via `NODEPULSE_API_KEY`:
  ```bash
  export NODEPULSE_API_KEY="np_live_8f3a92b0c1e84d2f83a5e1029c7b419d"
  ```
- **Redaction Invariant (BR-002)**:
  NodePulse will never log the plain API key in logs, terminal outputs, or exception messages.

---

## 4. Implementation: Constant-Time Comparison

To prevent timing side-channel attacks where an attacker deduces the API key byte-by-byte by observing minor variations in response latency, NodePulse verifies keys using constant-time string comparison:

```cpp
// src/utils/security_utils.cpp
namespace nodepulse::utils {

bool constant_time_equals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    volatile unsigned char result = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        result |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return result == 0;
}

} // namespace nodepulse::utils
```

---

## 5. Client Request Examples

### 5.1 cURL
```bash
curl -H "X-API-Key: np_live_8f3a92b0c1e84d2f83a5e1029c7b419d" \
     http://127.0.0.1:8080/api/v1/cpu
```

### 5.2 Python (`requests`)
```python
import requests

headers = {
    "X-API-Key": "np_live_8f3a92b0c1e84d2f83a5e1029c7b419d"
}
response = requests.get("http://127.0.0.1:8080/api/v1/system", headers=headers)
print(response.json())
```

### 5.3 JavaScript (`fetch`)
```javascript
const response = await fetch("http://127.0.0.1:8080/api/v1/memory", {
  headers: {
    "X-API-Key": "np_live_8f3a92b0c1e84d2f83a5e1029c7b419d"
  }
});
const data = await response.json();
console.log(data);
```

---

## 6. Authentication Failure Responses

When a client provides an invalid or missing API key, NodePulse immediately responds with HTTP 401 `UNAUTHORIZED`:

```http
HTTP/1.1 401 Unauthorized
Content-Type: application/json

{
  "error": {
    "code": "UNAUTHORIZED",
    "message": "Missing or invalid X-API-Key header",
    "timestamp": "2026-10-06T06:53:50Z",
    "details": []
  }
}
```

