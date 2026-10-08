import { describe, expect, it } from 'vitest';
import {
  EMPTY_FALLBACK,
  formatBytes,
  formatDurationSince,
  formatEpochSeconds,
  formatRate,
  formatUptime,
} from './formatters';

describe('formatters utility', () => {
  describe('formatBytes', () => {
    it('returns fallback for null, undefined, NaN, and negative numbers', () => {
      expect(formatBytes(null)).toBe(EMPTY_FALLBACK);
      expect(formatBytes(undefined)).toBe(EMPTY_FALLBACK);
      expect(formatBytes(NaN)).toBe(EMPTY_FALLBACK);
      expect(formatBytes(-1)).toBe(EMPTY_FALLBACK);
      expect(formatBytes(-1024)).toBe(EMPTY_FALLBACK);
    });

    it('formats 0 bytes correctly', () => {
      expect(formatBytes(0)).toBe('0 B');
    });

    it('formats bytes under 1 KB without decimals', () => {
      expect(formatBytes(500)).toBe('500 B');
      expect(formatBytes(1023)).toBe('1023 B');
    });

    it('formats KB, MB, GB, TB, and PB using base 1024', () => {
      expect(formatBytes(1024)).toBe('1.0 KB');
      expect(formatBytes(1536)).toBe('1.5 KB');
      expect(formatBytes(1048576)).toBe('1.0 MB');
      expect(formatBytes(1073741824)).toBe('1.0 GB');
      expect(formatBytes(1099511627776)).toBe('1.0 TB');
    });

    it('respects custom decimal places', () => {
      expect(formatBytes(1536, 0)).toBe('2 KB');
      expect(formatBytes(1536, 2)).toBe('1.50 KB');
      expect(formatBytes(1073741824 * 2.55, 2)).toBe('2.55 GB');
    });
  });

  describe('formatRate', () => {
    it('returns fallback for null, undefined, NaN, and negative numbers', () => {
      expect(formatRate(null)).toBe(EMPTY_FALLBACK);
      expect(formatRate(undefined)).toBe(EMPTY_FALLBACK);
      expect(formatRate(NaN)).toBe(EMPTY_FALLBACK);
      expect(formatRate(-1)).toBe(EMPTY_FALLBACK);
      expect(formatRate(-500)).toBe(EMPTY_FALLBACK);
    });

    it('formats 0 rate correctly', () => {
      expect(formatRate(0)).toBe('0 B/s');
    });

    it('formats rates under 1 KB/s without decimals', () => {
      expect(formatRate(450)).toBe('450 B/s');
    });

    it('formats KB/s, MB/s, GB/s using base 1024', () => {
      expect(formatRate(1024)).toBe('1.0 KB/s');
      expect(formatRate(1048576 * 2.4)).toBe('2.4 MB/s');
      expect(formatRate(1073741824 * 10)).toBe('10.0 GB/s');
    });

    it('respects custom decimal places', () => {
      expect(formatRate(1536, 0)).toBe('2 KB/s');
      expect(formatRate(1536, 2)).toBe('1.50 KB/s');
    });
  });

  describe('formatUptime', () => {
    it('returns fallback for null, undefined, NaN, and negative numbers', () => {
      expect(formatUptime(null)).toBe(EMPTY_FALLBACK);
      expect(formatUptime(undefined)).toBe(EMPTY_FALLBACK);
      expect(formatUptime(NaN)).toBe(EMPTY_FALLBACK);
      expect(formatUptime(-10)).toBe(EMPTY_FALLBACK);
    });

    it('formats 0 seconds as 0d 00h 00m', () => {
      expect(formatUptime(0)).toBe('0d 00h 00m');
    });

    it('formats durations under 1 hour', () => {
      expect(formatUptime(45)).toBe('0d 00h 00m');
      expect(formatUptime(65)).toBe('0d 00h 01m');
      expect(formatUptime(1800)).toBe('0d 00h 30m');
    });

    it('formats durations under 1 day with 2-digit padding', () => {
      expect(formatUptime(3665)).toBe('0d 01h 01m');
      expect(formatUptime(36000)).toBe('0d 10h 00m');
    });

    it('formats multi-day durations accurately', () => {
      // 14 days, 6 hours, 32 minutes = 14*86400 + 6*3600 + 32*60 = 1209600 + 21600 + 1920 = 1233120
      expect(formatUptime(1233120)).toBe('14d 06h 32m');
    });
  });

  describe('formatEpochSeconds', () => {
    it('returns fallback for null, undefined, NaN, 0, and negative numbers', () => {
      expect(formatEpochSeconds(null)).toBe(EMPTY_FALLBACK);
      expect(formatEpochSeconds(undefined)).toBe(EMPTY_FALLBACK);
      expect(formatEpochSeconds(NaN)).toBe(EMPTY_FALLBACK);
      expect(formatEpochSeconds(0)).toBe(EMPTY_FALLBACK);
      expect(formatEpochSeconds(-100)).toBe(EMPTY_FALLBACK);
    });

    it('formats positive epoch seconds into readable UTC timestamp', () => {
      // 1700000000 = 2023-11-14T22:13:20.000Z
      const result = formatEpochSeconds(1700000000);
      expect(result).toContain('2023-11-14');
      expect(result).toContain('22:13:20');
      expect(result).toContain('UTC');
    });
  });

  describe('formatDurationSince', () => {
    it('returns fallback for null, undefined, NaN, 0, and negative numbers', () => {
      expect(formatDurationSince(null)).toBe(EMPTY_FALLBACK);
      expect(formatDurationSince(undefined)).toBe(EMPTY_FALLBACK);
      expect(formatDurationSince(NaN)).toBe(EMPTY_FALLBACK);
      expect(formatDurationSince(0)).toBe(EMPTY_FALLBACK);
      expect(formatDurationSince(-5)).toBe(EMPTY_FALLBACK);
    });

    it('calculates humanized elapsed duration', () => {
      const now = 1700001000;
      const start = 1700000000; // 1000 seconds ago = 0d 00h 16m
      expect(formatDurationSince(start, now)).toBe('0d 00h 16m');
    });
  });
});
