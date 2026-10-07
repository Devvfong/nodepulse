#!/usr/bin/env bash
# NodePulse — Install Script Runtime Dependency Preflight Verification Tests
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BINARY="${REPO_ROOT}/build/apps/server/nodepulse_server"

if [ ! -f "${BINARY}" ]; then
    echo "Error: ${BINARY} does not exist. Build the project first." >&2
    exit 1
fi

TEST_TMP="/tmp/nodepulse_preflight_test_$$"
mkdir -p "${TEST_TMP}"
trap 'rm -rf "${TEST_TMP}"' EXIT

echo "=== Running Install Script Preflight Tests ==="

# ---------------------------------------------------------------------------
# Test 1: Unresolved dependencies detected on live installation without DESTDIR
# ---------------------------------------------------------------------------
echo "Test 1: Unresolved dependency detected on live installation..."
LIVE_DIR="${TEST_TMP}/live_fail"
mkdir -p "${LIVE_DIR}"
set +e
OUTPUT=$(env -u LD_LIBRARY_PATH \
    PREFIX="${LIVE_DIR}/usr" \
    SYSCONFDIR="${LIVE_DIR}/etc/nodepulse" \
    SYSTEMD_DIR="${LIVE_DIR}/etc/systemd/system" \
    LOG_DIR="${LIVE_DIR}/var/log/nodepulse" \
    "${REPO_ROOT}/scripts/install.sh" "${BINARY}" 2>&1)
EXIT_CODE=$?
set -e

if [ "${EXIT_CODE}" -eq 0 ]; then
    echo "FAIL: Expected install.sh to fail on unresolved runtime dependencies, but it exited 0." >&2
    exit 1
fi

if ! echo "${OUTPUT}" | grep -q "Error: Unresolved runtime shared library dependencies detected"; then
    echo "FAIL: Missing expected error message about unresolved dependencies." >&2
    echo "Output was:" >&2
    echo "${OUTPUT}" >&2
    exit 1
fi

echo "  -> PASS: Non-zero exit code (${EXIT_CODE}) and correct error message."

# ---------------------------------------------------------------------------
# Test 2: Failure occurs BEFORE creating configuration, logs, or systemd unit
# ---------------------------------------------------------------------------
echo "Test 2: Failure aborts before installing configuration, logs, or systemd unit..."
if [ -f "${LIVE_DIR}/etc/systemd/system/nodepulse.service" ]; then
    echo "FAIL: systemd unit was created despite preflight failure." >&2
    exit 1
fi
if [ -f "${LIVE_DIR}/etc/nodepulse/config.json" ]; then
    echo "FAIL: config.json was created despite preflight failure." >&2
    exit 1
fi
if [ -d "${LIVE_DIR}/var/log/nodepulse" ]; then
    echo "FAIL: log directory was created despite preflight failure." >&2
    exit 1
fi

echo "  -> PASS: Systemd unit, configuration, and log directories were not created."

# ---------------------------------------------------------------------------
# Test 3: No developer paths introduced in error message
# ---------------------------------------------------------------------------
echo "Test 3: No developer paths introduced in error message..."
ERROR_OUTPUT=$(echo "${OUTPUT}" | sed -n '/^Error:/,$p')
if echo "${ERROR_OUTPUT}" | grep -E '/home/devqii|\.local' >/dev/null; then
    echo "FAIL: Error output contained developer path /home/devqii or .local:" >&2
    echo "${ERROR_OUTPUT}" | grep -E '/home/devqii|\.local' >&2
    exit 1
fi

echo "  -> PASS: Zero developer paths in error message."

# ---------------------------------------------------------------------------
# Test 4: DESTDIR staging remains usable
# ---------------------------------------------------------------------------
echo "Test 4: DESTDIR staging remains usable..."
STAGE_DIR="${TEST_TMP}/stage"
mkdir -p "${STAGE_DIR}"

STAGE_OUTPUT=$(env -u LD_LIBRARY_PATH \
    DESTDIR="${STAGE_DIR}" \
    PREFIX="/usr" \
    SYSCONFDIR="/etc/nodepulse" \
    SYSTEMD_DIR="/etc/systemd/system" \
    "${REPO_ROOT}/scripts/install.sh" "${BINARY}" 2>&1)

if [ ! -f "${STAGE_DIR}/usr/bin/nodepulse_server" ]; then
    echo "FAIL: Staged binary missing." >&2
    exit 1
fi
if [ ! -f "${STAGE_DIR}/etc/nodepulse/config.json" ]; then
    echo "FAIL: Staged config missing." >&2
    exit 1
fi
if [ ! -f "${STAGE_DIR}/etc/systemd/system/nodepulse.service" ]; then
    echo "FAIL: Staged systemd unit missing." >&2
    exit 1
fi
if ! echo "${STAGE_OUTPUT}" | grep -q "Notice (DESTDIR staging)"; then
    echo "FAIL: Expected informational notice in DESTDIR staging output." >&2
    exit 1
fi

echo "  -> PASS: DESTDIR staging completed with informational notice."

# ---------------------------------------------------------------------------
# Test 5: Dependency check passes when dependencies resolve
# ---------------------------------------------------------------------------
echo "Test 5: Dependency check passes when dependencies resolve..."
LIVE_PASS="${TEST_TMP}/live_pass"
mkdir -p "${LIVE_PASS}"

# Supply LD_LIBRARY_PATH so that dependencies resolve cleanly
LD_LIBRARY_PATH="${HOME}/.local/lib/x86_64-linux-gnu:${HOME}/.local/lib:${LD_LIBRARY_PATH:-}" \
    PREFIX="${LIVE_PASS}/usr" \
    SYSCONFDIR="${LIVE_PASS}/etc/nodepulse" \
    SYSTEMD_DIR="${LIVE_PASS}/etc/systemd/system" \
    LOG_DIR="${LIVE_PASS}/var/log/nodepulse" \
    "${REPO_ROOT}/scripts/install.sh" "${BINARY}" >/dev/null

if [ ! -f "${LIVE_PASS}/usr/bin/nodepulse_server" ] || \
   [ ! -f "${LIVE_PASS}/etc/nodepulse/config.json" ] || \
   [ ! -f "${LIVE_PASS}/etc/systemd/system/nodepulse.service" ]; then
    echo "FAIL: Target files not installed when dependencies resolved." >&2
    exit 1
fi

echo "  -> PASS: All components installed cleanly when dependencies resolved."

echo "=== All 5 Install Script Preflight Tests Passed ==="
