# NodePulse — Configuration Specification

This document details the configuration files, environment variable overrides, schema options, and validation mechanisms for NodePulse.

---

## 1. Configuration Strategy

NodePulse employs a hierarchical configuration model:
1. **Default Values**: Built into C++ runtime constants.
2. **Configuration File**: JSON configuration file loaded from `/etc/nodepulse/config.json` (or specified via `--config <path>`).
3. **Environment Variables**: Overrides matching configuration file keys (`NODEPULSE_*`).
4. **CLI Flags**: Direct flags passed at execution time (`--port`, `--validate-config`).

---

## 2. Configuration Schema (`config.json`)

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "threads": 4,
    "log_level": "info",
    "log_format": "json"
  },
  "security": {
    "api_key": "CHANGE_ME_SECURE_API_KEY",
    "constant_time_comparison": true
  },
  "rate_limiting": {
    "enabled": true,
    "requests_per_minute": 120,
    "burst_capacity": 30
  },
  "collectors": {
    "system": { "enabled": true },
    "cpu": { "enabled": true, "sample_interval_ms": 1000 },
    "memory": { "enabled": true },
    "disks": {
      "enabled": true,
      "ignored_fstypes": ["proc", "sysfs", "cgroup", "devpts", "tmpfs", "overlay"]
    },
    "network": { "enabled": true },
    "processes": { "enabled": true, "max_process_limit": 200 },
    "services": { "enabled": true },
    "docker": {
      "enabled": false,
      "socket_path": "/var/run/docker.sock",
      "timeout_ms": 2000
    }
  },
  "sse": {
    "enabled": true,
    "interval_ms": 1000
  },
  "prometheus": {
    "enabled": true,
    "require_auth": true
  },
  "postgres": {
    "enabled": false,
    "connection_string": "postgresql://nodepulse:password@localhost:5432/nodepulse_db",
    "snapshot_interval_seconds": 60
  }
}
```

---

## 3. Environment Variable Overrides

| Environment Variable | Target JSON Path | Default Value | Description |
|---|---|---|---|
| `NODEPULSE_CONFIG` | CLI `--config` | `/etc/nodepulse/config.json` | Path to JSON config file |
| `NODEPULSE_HOST` | `server.host` | `0.0.0.0` | Listening IP address |
| `NODEPULSE_PORT` | `server.port` | `8080` | Listening TCP port |
| `NODEPULSE_THREADS` | `server.threads` | Core count | Drogon I/O event threads |
| `NODEPULSE_LOG_LEVEL` | `server.log_level` | `info` | `trace`, `debug`, `info`, `warn`, `error` |
| `NODEPULSE_API_KEY` | `security.api_key` | None (required) | Secret API key for authentication |
| `NODEPULSE_RATE_LIMIT` | `rate_limiting.requests_per_minute` | `120` | Requests allowed per minute per IP |
| `NODEPULSE_DOCKER_ENABLED`| `collectors.docker.enabled` | `false` | Enable Docker socket inspection |
| `NODEPULSE_DOCKER_SOCKET` | `collectors.docker.socket_path` | `/var/run/docker.sock` | Path to Docker Unix domain socket |
| `NODEPULSE_PROMETHEUS_AUTH` | `prometheus.require_auth` | `true` | Secure-by-default; set `false` to permit unauthenticated scrapes |

---

## 4. Configuration Validation

NodePulse provides a dedicated command-line validation flag to test syntax and permissions before deploying:

```bash
/usr/local/bin/nodepulse_server --validate-config --config /etc/nodepulse/config.json
```

- Returns exit code `0` if JSON syntax is valid, all mandatory parameters exist, and socket files are accessible.
- Returns exit code `1` and outputs human-readable error descriptions if validation fails.

