#!/usr/bin/env bash
# NodePulse — Production Uninstallation Script
# Usage: [PREFIX=/usr/local] [SYSCONFDIR=/etc/nodepulse] [SYSTEMD_DIR=/etc/systemd/system] ./scripts/uninstall.sh [--purge]

set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
SYSCONFDIR="${SYSCONFDIR:-/etc/nodepulse}"
SYSTEMD_DIR="${SYSTEMD_DIR:-/etc/systemd/system}"
LOG_DIR="${DESTDIR:-}/var/log/nodepulse"

BIN_DEST="${DESTDIR:-}${PREFIX}/bin"
CONF_DEST="${DESTDIR:-}${SYSCONFDIR}"
UNIT_DEST="${DESTDIR:-}${SYSTEMD_DIR}"

PURGE=false
for arg in "$@"; do
    if [ "${arg}" = "--purge" ]; then
        PURGE=true
    fi
done

echo "=== Uninstalling NodePulse ==="

# 1. Stop and disable service if running on live system
if [ -z "${DESTDIR:-}" ] && [ "$(id -u)" -eq 0 ] && command -v systemctl >/dev/null 2>&1; then
    if systemctl is-active --quiet nodepulse 2>/dev/null; then
        echo "Stopping nodepulse service..."
        systemctl stop nodepulse 2>/dev/null || true
    fi
    if systemctl is-enabled --quiet nodepulse 2>/dev/null; then
        echo "Disabling nodepulse service..."
        systemctl disable nodepulse 2>/dev/null || true
    fi
fi

# 2. Remove systemd service unit
if [ -f "${UNIT_DEST}/nodepulse.service" ]; then
    echo "Removing systemd unit: ${UNIT_DEST}/nodepulse.service"
    rm -f "${UNIT_DEST}/nodepulse.service"
    if [ -z "${DESTDIR:-}" ] && [ "$(id -u)" -eq 0 ] && command -v systemctl >/dev/null 2>&1; then
        systemctl daemon-reload 2>/dev/null || true
    fi
fi

# 3. Remove binary
if [ -f "${BIN_DEST}/nodepulse_server" ]; then
    echo "Removing binary: ${BIN_DEST}/nodepulse_server"
    rm -f "${BIN_DEST}/nodepulse_server"
fi

# 4. Handle configuration and logs
if [ "${PURGE}" = true ]; then
    echo "Purge requested: removing configuration and logs..."
    if [ -d "${CONF_DEST}" ]; then
        echo "Removing configuration directory: ${CONF_DEST}"
        rm -rf "${CONF_DEST}"
    fi
    if [ -d "${LOG_DIR}" ]; then
        echo "Removing log directory: ${LOG_DIR}"
        rm -rf "${LOG_DIR}"
    fi
else
    echo "Preserving configuration at ${CONF_DEST} (use --purge to remove)."
    echo "Preserving log directory at ${LOG_DIR} (use --purge to remove)."
fi

echo "=== NodePulse Uninstallation Complete ==="

