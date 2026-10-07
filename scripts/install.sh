#!/usr/bin/env bash
# NodePulse — Production Installation Script
# Usage: [PREFIX=/usr/local] [SYSCONFDIR=/etc/nodepulse] [SYSTEMD_DIR=/etc/systemd/system] ./scripts/install.sh [binary_path]

set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
SYSCONFDIR="${SYSCONFDIR:-/etc/nodepulse}"
SYSTEMD_DIR="${SYSTEMD_DIR:-/etc/systemd/system}"
LOG_DIR="${LOG_DIR:-${DESTDIR:-}/var/log/nodepulse}"

BIN_DEST="${DESTDIR:-}${PREFIX}/bin"
CONF_DEST="${DESTDIR:-}${SYSCONFDIR}"
UNIT_DEST="${DESTDIR:-}${SYSTEMD_DIR}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Find binary to install
BINARY_SOURCE="${1:-}"
if [ -z "${BINARY_SOURCE}" ]; then
    if [ -f "${REPO_ROOT}/build/apps/server/nodepulse_server" ]; then
        BINARY_SOURCE="${REPO_ROOT}/build/apps/server/nodepulse_server"
    elif [ -f "${REPO_ROOT}/build-release/apps/server/nodepulse_server" ]; then
        BINARY_SOURCE="${REPO_ROOT}/build-release/apps/server/nodepulse_server"
    elif [ -f "${REPO_ROOT}/bin/nodepulse_server" ]; then
        BINARY_SOURCE="${REPO_ROOT}/bin/nodepulse_server"
    else
        echo "Error: nodepulse_server binary not found. Build the project first or provide the binary path as an argument." >&2
        exit 1
    fi
fi

if [ ! -f "${BINARY_SOURCE}" ]; then
    echo "Error: Binary '${BINARY_SOURCE}' does not exist or is not a file." >&2
    exit 1
fi

echo "=== Installing NodePulse ==="
echo "Binary source:       ${BINARY_SOURCE}"
echo "Binary destination:  ${BIN_DEST}/nodepulse_server"
echo "Configuration dest:  ${CONF_DEST}/config.json"
echo "Systemd unit dest:   ${UNIT_DEST}/nodepulse.service"
echo "Log directory dest:  ${LOG_DIR}"

# 1. Provision dedicated system user and group (only if on live system as root)
if [ -z "${DESTDIR:-}" ] && [ "$(id -u)" -eq 0 ]; then
    if ! getent group nodepulse >/dev/null 2>&1; then
        echo "Creating dedicated system group 'nodepulse'..."
        groupadd --system nodepulse
    fi
    if ! getent passwd nodepulse >/dev/null 2>&1; then
        echo "Creating dedicated system user 'nodepulse'..."
        useradd --system --gid nodepulse --no-create-home --shell /usr/sbin/nologin nodepulse
    fi
fi

# 2. Install binary
mkdir -p "${BIN_DEST}"
install -m 0755 "${BINARY_SOURCE}" "${BIN_DEST}/nodepulse_server"
echo "Installed binary: ${BIN_DEST}/nodepulse_server"

# 3. Verify runtime shared library dependencies
INSTALLED_BIN="${BIN_DEST}/nodepulse_server"
if command -v ldd >/dev/null 2>&1; then
    MISSING_LIBS=$(ldd "${INSTALLED_BIN}" 2>&1 | grep "=> not found" || true)
    if [ -n "${MISSING_LIBS}" ]; then
        if [ -n "${DESTDIR:-}" ]; then
            echo "Notice (DESTDIR staging): Staged binary has unresolved shared libraries outside staging environment:" >&2
            echo "${MISSING_LIBS}" | sed 's/^/  /' >&2
            echo "Ensure required runtime libraries are installed in target system paths before running." >&2
        else
            echo "Error: Unresolved runtime shared library dependencies detected for ${INSTALLED_BIN}:" >&2
            echo "${MISSING_LIBS}" | sed 's/^/  /' >&2
            echo "" >&2
            echo "NodePulse requires its shared runtime libraries to be installed in standard system" >&2
            echo "library paths (e.g. /usr/lib, /usr/local/lib) and registered with ldconfig." >&2
            echo "Please install all missing runtime dependencies on the target host before installing NodePulse." >&2
            exit 1
        fi
    fi
fi

# 4. Install configuration (do NOT overwrite existing config)
mkdir -p "${CONF_DEST}"
CONFIG_TEMPLATE="${REPO_ROOT}/config/config.example.json"
if [ -f "${CONFIG_TEMPLATE}" ]; then
    install -m 0640 "${CONFIG_TEMPLATE}" "${CONF_DEST}/config.example.json"
    echo "Installed reference configuration: ${CONF_DEST}/config.example.json"
    if [ ! -f "${CONF_DEST}/config.json" ]; then
        install -m 0640 "${CONFIG_TEMPLATE}" "${CONF_DEST}/config.json"
        echo "Installed default configuration: ${CONF_DEST}/config.json"
        if [ -z "${DESTDIR:-}" ] && [ "$(id -u)" -eq 0 ]; then
            chown root:nodepulse "${CONF_DEST}/config.json" "${CONF_DEST}/config.example.json" 2>/dev/null || true
        fi
    else
        echo "Preserved existing configuration: ${CONF_DEST}/config.json"
    fi
fi

# 5. Create log directory
mkdir -p "${LOG_DIR}"
if [ -z "${DESTDIR:-}" ] && [ "$(id -u)" -eq 0 ]; then
    chown nodepulse:nodepulse "${LOG_DIR}" 2>/dev/null || true
    chmod 0750 "${LOG_DIR}"
fi
echo "Created log directory: ${LOG_DIR}"

# 6. Install systemd service unit
mkdir -p "${UNIT_DEST}"
if [ -f "${REPO_ROOT}/infrastructure/systemd/nodepulse.service.in" ]; then
    sed -e "s|@NODEPULSE_INSTALL_FULL_BINDIR@|${PREFIX}/bin|g" \
        -e "s|@NODEPULSE_CONFIG_FILE@|${SYSCONFDIR}/config.json|g" \
        -e "s|@NODEPULSE_CONFIG_DIR@|${SYSCONFDIR}|g" \
        "${REPO_ROOT}/infrastructure/systemd/nodepulse.service.in" > "${UNIT_DEST}/nodepulse.service"
    chmod 0644 "${UNIT_DEST}/nodepulse.service"
    echo "Generated and installed systemd unit: ${UNIT_DEST}/nodepulse.service"
elif [ -f "${REPO_ROOT}/build/infrastructure/systemd/nodepulse.service" ]; then
    install -m 0644 "${REPO_ROOT}/build/infrastructure/systemd/nodepulse.service" "${UNIT_DEST}/nodepulse.service"
    echo "Installed systemd unit: ${UNIT_DEST}/nodepulse.service"
elif [ -f "${REPO_ROOT}/infrastructure/systemd/nodepulse.service" ]; then
    install -m 0644 "${REPO_ROOT}/infrastructure/systemd/nodepulse.service" "${UNIT_DEST}/nodepulse.service"
    echo "Installed systemd unit: ${UNIT_DEST}/nodepulse.service"
fi

# 7. Reload systemd daemon if on live system as root
if [ -z "${DESTDIR:-}" ] && [ "$(id -u)" -eq 0 ] && command -v systemctl >/dev/null 2>&1; then
    if systemctl is-system-running >/dev/null 2>&1 || [ -d /run/systemd/system ]; then
        echo "Reloading systemd manager configuration..."
        systemctl daemon-reload
    fi
fi

echo ""
echo "=== NodePulse Installation Complete ==="
echo "Next steps:"
echo "1. Set a secure API key in ${CONF_DEST}/config.json or configure NODEPULSE_API_KEY environment variable."
echo "2. Validate configuration:"
echo "   ${BIN_DEST}/nodepulse_server --validate-config --config ${CONF_DEST}/config.json"
echo "3. Enable and start the service:"
echo "   sudo systemctl enable --now nodepulse"
echo "4. Verify service status and logs:"
echo "   sudo systemctl status nodepulse"
echo "   journalctl -u nodepulse -f"

