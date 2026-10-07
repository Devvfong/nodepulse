# NodePulse — Troubleshooting & Diagnostic Guide

This document outlines diagnostic procedures and resolutions for common failure scenarios encountered when running NodePulse.

---

## 1. Quick Diagnostic Checklist

When experiencing issues with NodePulse:
1. **Check Service Vitality**: `sudo systemctl status nodepulse`
2. **Inspect Journal Logs**: `journalctl -u nodepulse -n 100 --no-pager`
3. **Verify Configuration**: `/usr/local/bin/nodepulse_server --validate-config --config /etc/nodepulse/config.json`
4. **Test Local Unauthenticated Health Check**: `curl -v http://127.0.0.1:8080/api/v1/health`
5. **Verify Process Socket Binding**: `ss -tulpn | grep 8080`

---

## 2. Common Failure Scenarios & Resolutions

### 2.1 Agent Fails to Start: "Address already in use"
- **Symptoms**: Service transitions to `failed` state immediately upon startup. `journalctl` shows `bind error: Address already in use`.
- **Cause**: Another service or previous instance of NodePulse is already bound to port 8080.
- **Resolution**:
  ```bash
  # Identify process occupying port 8080
  sudo ss -tulpn | grep 8080
  # Update listening port in /etc/nodepulse/config.json or stop conflicting process
  ```

---

### 2.2 Requests Fail with HTTP 401 `UNAUTHORIZED`
- **Symptoms**: Client requests receive HTTP 401 with body `Missing or invalid X-API-Key header`.
- **Cause**:
  1. Header was omitted or misspelled (e.g. `X-Api-Key` vs `X-API-Key`).
  2. The supplied key does not match the key defined in `/etc/nodepulse/config.json` or `NODEPULSE_API_KEY`.
- **Resolution**:
  ```bash
  # Test with explicit curl header
  curl -v -H "X-API-Key: YOUR_CONFIGURED_KEY" http://127.0.0.1:8080/api/v1/system
  ```

---

### 2.3 Requests Fail with HTTP 429 `RATE_LIMITED`
- **Symptoms**: Clients intermittently receive HTTP 429 with `Retry-After: 1`.
- **Cause**: Inbound request frequency from the client IP exceeded `requests_per_minute` (default 120) or burst capacity (default 30).
- **Resolution**:
  - Tune `rate_limiting.requests_per_minute` in `/etc/nodepulse/config.json` to a higher threshold.
  - Implement exponential backoff in client scraping scripts respecting the `Retry-After` header.

---

### 2.4 Calls to `/api/v1/containers` Return HTTP 503 `DOCKER_UNAVAILABLE`
- **Symptoms**: Docker endpoint fails with HTTP 503 while all other host metrics succeed.
- **Cause**:
  1. Docker daemon is stopped (`systemctl status docker`).
  2. `/var/run/docker.sock` does not exist or user `nodepulse` lacks read/write permissions to the socket.
- **Resolution**:
  ```bash
  # Check socket existence and permissions
  ls -la /var/run/docker.sock
  # Ensure nodepulse user is a member of the docker group
  sudo usermod -aG docker nodepulse
  sudo systemctl restart nodepulse
  ```

---

### 2.5 `/api/v1/disks` Returns an Empty Array `[]`
- **Symptoms**: Endpoint returns 200 OK, but no partitions are listed.
- **Cause**: All mounted partitions match the `ignored_fstypes` filter list (`tmpfs`, `overlay`, `cgroup`, etc.), or NodePulse is running in a restricted container mount namespace where `/proc/mounts` is inaccessible.
- **Resolution**:
  - Verify contents of `/proc/mounts`.
  - Adjust `collectors.disks.ignored_fstypes` in `/etc/nodepulse/config.json` if monitoring custom block filesystem types.

---

### 2.6 High Agent CPU Usage (> 2%)
- **Symptoms**: `top` indicates `nodepulse_server` is consuming significant CPU.
- **Cause**: Frequent queries to `/api/v1/processes` on a machine with thousands of running processes, or excessively frequent polling (< 100ms interval).
- **Resolution**:
  - Ensure client poll intervals are >= 1000ms.
  - Constrain process list queries using `?limit=20` rather than unbound process queries.

---

### 2.7 Service Fails to Start: Unresolved Shared Runtime Libraries
- **Symptoms**: `nodepulse.service` fails immediately upon startup, `journalctl -u nodepulse` reports `error while loading shared libraries: <name>.so: cannot open shared object file: No such file or directory`, or `scripts/install.sh` aborts with `Error: Unresolved runtime shared library dependencies detected`.
- **Cause**: The production binary was compiled with dynamic linking to core shared libraries (`libdrogon.so.1`, `libtrantor.so.1`, `libjsoncpp`, `libspdlog`, `libfmt`, `libpq.so.5`), but one or more of these libraries is missing from the target host or located in a directory not indexed by the dynamic linker.
- **Resolution**:
  1. Inspect the binary dependencies to identify the missing libraries:
     ```bash
     ldd /usr/local/bin/nodepulse_server
     ```
  2. Install the corresponding runtime packages via your distribution's package manager.
  3. If libraries were installed from source into `/usr/local/lib` or another custom path, ensure that path is listed in a file under `/etc/ld.so.conf.d/` (e.g., `/etc/ld.so.conf.d/local.conf`) and refresh the dynamic linker cache:
     ```bash
     sudo ldconfig
     ```
  4. Re-verify with `ldd /usr/local/bin/nodepulse_server` that zero libraries report `=> not found`, then restart the service:
     ```bash
     sudo systemctl restart nodepulse
     ```

