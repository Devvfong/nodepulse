export const EMPTY_FALLBACK = ' — ';

const BYTE_UNITS = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
const RATE_UNITS = ['B/s', 'KB/s', 'MB/s', 'GB/s', 'TB/s'];

/**
 * Converts bytes to humanized B, KB, MB, GB, TB with base 1024.
 * Returns fallback placeholder for null/undefined/negative values.
 */
export function formatBytes(
  bytes: number | null | undefined,
  decimals = 1,
): string {
  if (
    bytes === null ||
    bytes === undefined ||
    bytes < 0 ||
    Number.isNaN(bytes)
  ) {
    return EMPTY_FALLBACK;
  }

  if (bytes === 0) {
    return '0 B';
  }

  const i = Math.min(
    Math.floor(Math.log(bytes) / Math.log(1024)),
    BYTE_UNITS.length - 1,
  );

  if (i === 0) {
    return `${Math.round(bytes)} B`;
  }

  const val = bytes / Math.pow(1024, i);
  return `${val.toFixed(decimals)} ${BYTE_UNITS[i]}`;
}

/**
 * Converts rate in bytes per second to B/s, KB/s, MB/s, GB/s with base 1024.
 * Returns fallback placeholder for null/undefined/negative values.
 */
export function formatRate(
  bytesPerSec: number | null | undefined,
  decimals = 1,
): string {
  if (
    bytesPerSec === null ||
    bytesPerSec === undefined ||
    bytesPerSec < 0 ||
    Number.isNaN(bytesPerSec)
  ) {
    return EMPTY_FALLBACK;
  }

  if (bytesPerSec === 0) {
    return '0 B/s';
  }

  const i = Math.min(
    Math.floor(Math.log(bytesPerSec) / Math.log(1024)),
    RATE_UNITS.length - 1,
  );

  if (i === 0) {
    return `${Math.round(bytesPerSec)} B/s`;
  }

  const val = bytesPerSec / Math.pow(1024, i);
  return `${val.toFixed(decimals)} ${RATE_UNITS[i]}`;
}

/**
 * Converts duration in seconds to humanized `${d}d ${h}h ${m}m`.
 * Returns fallback placeholder for null/undefined/negative values.
 */
export function formatUptime(seconds: number | null | undefined): string {
  if (
    seconds === null ||
    seconds === undefined ||
    seconds < 0 ||
    Number.isNaN(seconds)
  ) {
    return EMPTY_FALLBACK;
  }

  const totalSec = Math.floor(seconds);
  const days = Math.floor(totalSec / 86400);
  const hours = Math.floor((totalSec % 86400) / 3600);
  const minutes = Math.floor((totalSec % 3600) / 60);

  const hStr = String(hours).padStart(2, '0');
  const mStr = String(minutes).padStart(2, '0');

  return `${days}d ${hStr}h ${mStr}m`;
}

/**
 * Converts epoch seconds to humanized UTC string (`YYYY-MM-DD HH:mm:ss UTC`).
 * Returns fallback placeholder for null/undefined/<=0 values.
 */
export function formatEpochSeconds(
  epochSec: number | null | undefined,
): string {
  if (
    epochSec === null ||
    epochSec === undefined ||
    epochSec <= 0 ||
    Number.isNaN(epochSec)
  ) {
    return EMPTY_FALLBACK;
  }

  const d = new Date(epochSec * 1000);
  if (Number.isNaN(d.getTime())) {
    return EMPTY_FALLBACK;
  }

  const iso = d.toISOString();
  const datePart = iso.substring(0, 10);
  const timePart = iso.substring(11, 19);

  return `${datePart} ${timePart} UTC`;
}

/**
 * Converts epoch seconds to humanized duration elapsed since then.
 */
export function formatDurationSince(
  epochSec: number | null | undefined,
  nowSec: number = Math.floor(Date.now() / 1000),
): string {
  if (
    epochSec === null ||
    epochSec === undefined ||
    epochSec <= 0 ||
    Number.isNaN(epochSec)
  ) {
    return EMPTY_FALLBACK;
  }

  const elapsed = Math.max(0, nowSec - epochSec);
  return formatUptime(elapsed);
}
