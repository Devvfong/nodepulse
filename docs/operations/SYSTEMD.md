# NodePulse — Systemd Service Unit & Hardening Guide

This document specifies the systemd service unit configuration (`nodepulse.service`), process sandboxing directives, and operational management procedures.

---

## 1. Systemd Service Unit Specification

File location: `/etc/systemd/system/nodepulse.service` (or `infrastructure/systemd/nodepulse.service` in the repository).

```ini
[Unit]
Description=NodePulse Linux Host Monitoring & Management Agent
Documentation=https://github.com/nodepulse/nodepulse
After=network.target network-online.target
Wants=network-online.target

[Service]
Type=simple
User=nodepulse
Group=nodepulse
ExecStart=/usr/local/bin/nodepulse_server --config /etc/nodepulse/config.json
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5s
KillMode=mixed
TimeoutStopSec=10s

# Process & Privilege Hardening
NoNewPrivileges=true
CapabilityBoundingSet=
AmbientCapabilities=

# Filesystem Protections
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
ReadOnlyPaths=/proc /sys /etc/os-release /etc/nodepulse
ReadWritePaths=/var/log/nodepulse
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true

# Memory & Kernel Protections
MemoryDenyWriteExecute=true
LockPersonality=true
RestrictRealtime=true
RestrictSUIDSGID=true

# Networking Protections
RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX

# Resource Limits
LimitNOFILE=65535
TasksMax=128

[Install]
WantedBy=multi-user.target
```

---

## 2. Hardening Directives Explained

| Directive | Security Objective |
|---|---|
| **`NoNewPrivileges=true`** | Prevents child processes from acquiring new privileges via `setuid`/`setgid` binaries. |
| **`CapabilityBoundingSet=`** | Strips all Linux POSIX capabilities (e.g. `CAP_SYS_ADMIN`, `CAP_NET_ADMIN`). |
| **`ProtectSystem=strict`** | Mounts the entire filesystem `/` read-only for the process, except explicitly declared `ReadWritePaths`. |
| **`ProtectHome=true`** | Makes `/home`, `/root`, and `/run/user` completely inaccessible and empty. |
| **`PrivateTmp=true`** | Allocates an isolated, private `/tmp` and `/var/tmp` directory inside a mount namespace. |
| **`MemoryDenyWriteExecute=true`** | Prohibits creating memory mappings that are simultaneously writable and executable (prevents shellcode execution). |
| **`RestrictAddressFamilies`** | Restricts network sockets strictly to IPv4, IPv6, and Unix domain sockets (`AF_UNIX`). |

---

## 3. Operational Service Management

### Common Commands
```bash
# Reload unit definitions after editing unit file
sudo systemctl daemon-reload

# Enable service to start automatically at boot
sudo systemctl enable nodepulse

# Start the service
sudo systemctl start nodepulse

# Inspect runtime status and active PID
sudo systemctl status nodepulse

# Restart the service
sudo systemctl restart nodepulse

# Stop the service
sudo systemctl stop nodepulse
```

### Inspecting Service Logs
```bash
# Stream live logs in real time
journalctl -u nodepulse -f

# View logs from the current boot with priority error or warning
journalctl -u nodepulse -b -p err..warning
```

### Verifying Systemd Security Posture
Inspect the security score assigned by systemd's built-in analyzer:
```bash
systemd-analyze security nodepulse
```
*A fully hardened NodePulse unit should achieve an exposure score < 2.0 (OK / PASS).*

