import { TelemetrySample } from '../models/pulse.model';

export const MAX_TELEMETRY_SAMPLES = 60;

/**
 * Creates a TelemetrySample object preserving timestamps and null values faithfully.
 */
export function createTelemetrySample<T>(
  timestamp: string,
  value: T | null,
): TelemetrySample<T> {
  return {
    timestamp,
    value,
  };
}

/**
 * Appends a new sample to a telemetry buffer, returning a new immutable array
 * capped at the last `maxSamples` entries.
 */
export function appendTelemetrySample<T>(
  buffer: readonly TelemetrySample<T>[],
  sample: TelemetrySample<T>,
  maxSamples: number = MAX_TELEMETRY_SAMPLES,
): TelemetrySample<T>[] {
  if (maxSamples <= 0) {
    return [];
  }
  const updated = [...buffer, sample];
  return updated.length > maxSamples ? updated.slice(-maxSamples) : updated;
}
