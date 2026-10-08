import { TelemetrySample } from '../models/pulse.model';
import {
  MAX_TELEMETRY_SAMPLES,
  appendTelemetrySample,
  createTelemetrySample,
} from './telemetry-buffer.util';

describe('telemetry-buffer.util', () => {
  describe('MAX_TELEMETRY_SAMPLES', () => {
    it('should be defined as 60', () => {
      expect(MAX_TELEMETRY_SAMPLES).toBe(60);
    });
  });

  describe('createTelemetrySample', () => {
    it('should construct a valid TelemetrySample with timestamp and value', () => {
      const sample = createTelemetrySample('2026-10-08T12:00:00Z', 42.5);
      expect(sample).toEqual({
        timestamp: '2026-10-08T12:00:00Z',
        value: 42.5,
      });
    });

    it('should preserve null values without coercing to 0 or NaN', () => {
      const sample = createTelemetrySample<number>('2026-10-08T12:00:01Z', null);
      expect(sample).toEqual({
        timestamp: '2026-10-08T12:00:01Z',
        value: null,
      });
      expect(sample.value).toBeNull();
      expect(sample.value).not.toBe(0);
      expect(Number.isNaN(sample.value)).toBe(false);
    });

    it('should preserve timestamp string unmodified', () => {
      const isoString = '2026-10-08T14:32:01.123456Z';
      const sample = createTelemetrySample(isoString, 100);
      expect(sample.timestamp).toBe(isoString);
    });
  });

  describe('appendTelemetrySample', () => {
    it('should append a sample to an empty buffer and return a 1-element array', () => {
      const buffer: TelemetrySample<number>[] = [];
      const sample = createTelemetrySample('2026-10-08T12:00:00Z', 10);

      const result = appendTelemetrySample(buffer, sample);

      expect(result).toHaveLength(1);
      expect(result[0]).toEqual(sample);
      expect(result).not.toBe(buffer);
    });

    it('should append samples sequentially up to MAX_TELEMETRY_SAMPLES (60) without dropping any', () => {
      let buffer: TelemetrySample<number>[] = [];

      for (let i = 1; i <= 60; i++) {
        const sample = createTelemetrySample(`2026-10-08T12:00:${i.toString().padStart(2, '0')}Z`, i);
        buffer = appendTelemetrySample(buffer, sample);
      }

      expect(buffer).toHaveLength(60);
      expect(buffer[0].value).toBe(1);
      expect(buffer[59].value).toBe(60);
    });

    it('should strictly cap array at 60 and shift the oldest sample out when appending the 61st sample', () => {
      let buffer: TelemetrySample<number>[] = [];

      for (let i = 1; i <= 60; i++) {
        const sample = createTelemetrySample(`2026-10-08T12:00:${i.toString().padStart(2, '0')}Z`, i);
        buffer = appendTelemetrySample(buffer, sample);
      }

      expect(buffer).toHaveLength(60);
      expect(buffer[0].value).toBe(1);

      // Append 61st sample
      const sample61 = createTelemetrySample('2026-10-08T12:01:01Z', 61);
      buffer = appendTelemetrySample(buffer, sample61);

      expect(buffer).toHaveLength(60);
      // Sample 1 dropped, sample 2 is now first
      expect(buffer[0].value).toBe(2);
      expect(buffer[0].timestamp).toBe('2026-10-08T12:00:02Z');
      // Sample 61 is now last
      expect(buffer[59].value).toBe(61);
      expect(buffer[59].timestamp).toBe('2026-10-08T12:01:01Z');
    });

    it('should preserve value === null honestly across buffer operations', () => {
      const sample1 = createTelemetrySample('2026-10-08T12:00:01Z', 12.5);
      const sample2 = createTelemetrySample<number>('2026-10-08T12:00:02Z', null);
      const sample3 = createTelemetrySample('2026-10-08T12:00:03Z', 25.0);

      let buffer: TelemetrySample<number>[] = [];
      buffer = appendTelemetrySample(buffer, sample1);
      buffer = appendTelemetrySample(buffer, sample2);
      buffer = appendTelemetrySample(buffer, sample3);

      expect(buffer).toHaveLength(3);
      expect(buffer[1].value).toBeNull();
      expect(buffer[1].value).not.toBe(0);
      expect(Number.isNaN(buffer[1].value)).toBe(false);
      expect(buffer[1].timestamp).toBe('2026-10-08T12:00:02Z');
    });

    it('should preserve timestamps accurately without alteration', () => {
      const timestamps = [
        '2026-10-08T12:00:00Z',
        '2026-10-08T12:00:01.500Z',
        '2026-10-08T12:00:03.000Z',
      ];
      let buffer: TelemetrySample<number>[] = [];

      timestamps.forEach((ts, idx) => {
        buffer = appendTelemetrySample(buffer, createTelemetrySample(ts, idx * 10));
      });

      expect(buffer.map((s) => s.timestamp)).toEqual(timestamps);
    });

    it('should return a new array instance without mutating the input buffer (pure/immutable)', () => {
      const initial: readonly TelemetrySample<number>[] = Object.freeze([
        createTelemetrySample('2026-10-08T12:00:00Z', 1),
      ]);
      const sample = createTelemetrySample('2026-10-08T12:00:01Z', 2);

      const result = appendTelemetrySample(initial, sample);

      expect(result).not.toBe(initial);
      expect(initial).toHaveLength(1);
      expect(result).toHaveLength(2);
    });

    it('should respect custom maxSamples parameter', () => {
      let buffer: TelemetrySample<string>[] = [];
      const customMax = 5;

      for (let i = 1; i <= 10; i++) {
        buffer = appendTelemetrySample(
          buffer,
          createTelemetrySample(`t-${i}`, `val-${i}`),
          customMax,
        );
      }

      expect(buffer).toHaveLength(customMax);
      expect(buffer[0].value).toBe('val-6');
      expect(buffer[4].value).toBe('val-10');
    });

    it('should handle maxSamples <= 0 by returning an empty array', () => {
      const buffer = [createTelemetrySample('t-1', 1)];
      const sample = createTelemetrySample('t-2', 2);

      expect(appendTelemetrySample(buffer, sample, 0)).toEqual([]);
      expect(appendTelemetrySample(buffer, sample, -1)).toEqual([]);
    });
  });
});
