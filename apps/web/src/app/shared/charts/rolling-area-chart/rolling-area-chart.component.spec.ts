import { ComponentFixture, TestBed } from '@angular/core/testing';
import { TelemetrySample } from '../../../core/models/pulse.model';
import { RollingAreaChartComponent } from './rolling-area-chart.component';

describe('RollingAreaChartComponent', () => {
  let component: RollingAreaChartComponent;
  let fixture: ComponentFixture<RollingAreaChartComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [RollingAreaChartComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(RollingAreaChartComponent);
    component = fixture.componentInstance;
  });

  it('should create the rolling area chart component', () => {
    expect(component).toBeTruthy();
  });

  describe('default inputs and SVG DOM structure', () => {
    it('should have correct default inputs and SVG attributes', () => {
      fixture.detectChanges();
      const svgEl = fixture.nativeElement.querySelector('svg');
      const defsEl = fixture.nativeElement.querySelector('defs');
      const primaryLineEl = fixture.nativeElement.querySelector('.primary-line');

      expect(svgEl).toBeTruthy();
      expect(defsEl).toBeTruthy();
      expect(primaryLineEl).toBeTruthy();
      expect(svgEl.getAttribute('viewBox')).toBe('0 0 300 80');
      expect(svgEl.getAttribute('preserveAspectRatio')).toBe('none');
      expect(svgEl.classList.contains('w-full')).toBe(true);
      expect(svgEl.classList.contains('overflow-visible')).toBe(true);
      expect(primaryLineEl.getAttribute('stroke')).toBe('#34d399');
      expect(primaryLineEl.getAttribute('stroke-width')).toBe('1.5');
    });

    it('should not render legend when labels are omitted', () => {
      fixture.detectChanges();
      const legendEl = fixture.nativeElement.querySelector('[data-testid="chart-legend"]');
      expect(legendEl).toBeNull();
    });
  });

  describe('test case (a): empty samples and all-null arrays', () => {
    it('should render a neutral flat baseline path when samples array is empty', () => {
      fixture.componentRef.setInput('samples', []);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const areaEl = fixture.nativeElement.querySelector('.primary-area');
      const d = lineEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      // For width=300, height=80, padding=4 -> baseline Y is 76
      expect(d).toBe('M 0 76 L 300 76');
      // No area fill when empty
      expect(areaEl).toBeNull();
    });

    it('should render a neutral flat baseline path when samples contain only null values', () => {
      const nullSamples: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: null },
        { timestamp: '2026-10-08T12:00:01Z', value: null },
        { timestamp: '2026-10-08T12:00:02Z', value: null },
      ];
      fixture.componentRef.setInput('samples', nullSamples);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const areaEl = fixture.nativeElement.querySelector('.primary-area');
      const d = lineEl.getAttribute('d');

      expect(d).toBe('M 0 76 L 300 76');
      expect(areaEl).toBeNull();
    });
  });

  describe('test case (b): single trace input generates valid area and line path strings without NaN', () => {
    it('should generate valid area and line path d strings for monotonic samples', () => {
      const samples: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 10 },
        { timestamp: '2026-10-08T12:00:01Z', value: 20 },
        { timestamp: '2026-10-08T12:00:02Z', value: 30 },
      ];
      fixture.componentRef.setInput('samples', samples);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const areaEl = fixture.nativeElement.querySelector('.primary-area');

      expect(lineEl).toBeTruthy();
      expect(areaEl).toBeTruthy();

      const lineD = lineEl.getAttribute('d');
      const areaD = areaEl.getAttribute('d');

      expect(lineD).toBeTruthy();
      expect(lineD).not.toContain('NaN');
      expect(lineD).not.toContain('Infinity');
      // x values: 0, 150, 300. y values for min=10, max=30, range=20, availableHeight=72:
      // val 10 -> y = 80 - 4 - 0 = 76
      // val 20 -> y = 80 - 4 - 36 = 40
      // val 30 -> y = 80 - 4 - 72 = 4
      expect(lineD).toBe('M 0 76 L 150 40 L 300 4');

      expect(areaD).toBeTruthy();
      expect(areaD).not.toContain('NaN');
      expect(areaD).not.toContain('Infinity');
      // Area closes down to baseline 76: L lastX 76 L firstX 76 Z
      expect(areaD).toBe('M 0 76 L 150 40 L 300 4 L 300 76 L 0 76 Z');

      // Verify fill references gradient
      expect(areaEl.getAttribute('fill')).toContain('url(#');
    });

    it('should handle a single valid sample without division by zero or NaN', () => {
      const samples: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 42 },
      ];
      fixture.componentRef.setInput('samples', samples);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const areaEl = fixture.nativeElement.querySelector('.primary-area');

      const lineD = lineEl.getAttribute('d');
      const areaD = areaEl.getAttribute('d');

      expect(lineD).toBeTruthy();
      expect(lineD).not.toContain('NaN');
      expect(lineD).not.toContain('Infinity');
      expect(lineD).toBe('M 0 76');

      expect(areaD).toBeTruthy();
      expect(areaD).not.toContain('NaN');
      expect(areaD).not.toContain('Infinity');
      expect(areaD).toBe('M 0 76 L 0 76 L 0 76 Z');
    });
  });

  describe('test case (c): dual trace input generates separate primary and secondary paths with respective colors', () => {
    it('should render distinct primary and secondary traces with respective colors', () => {
      const primary: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 100 },
        { timestamp: '2026-10-08T12:00:01Z', value: 200 },
      ];
      const secondary: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 50 },
        { timestamp: '2026-10-08T12:00:01Z', value: 150 },
      ];

      fixture.componentRef.setInput('samples', primary);
      fixture.componentRef.setInput('secondarySamples', secondary);
      fixture.componentRef.setInput('primaryColor', '#10b981');
      fixture.componentRef.setInput('secondaryColor', '#06b6d4');
      fixture.detectChanges();

      const primaryLine = fixture.nativeElement.querySelector('.primary-line');
      const primaryArea = fixture.nativeElement.querySelector('.primary-area');
      const secondaryLine = fixture.nativeElement.querySelector('.secondary-line');
      const secondaryArea = fixture.nativeElement.querySelector('.secondary-area');

      expect(primaryLine).toBeTruthy();
      expect(primaryArea).toBeTruthy();
      expect(secondaryLine).toBeTruthy();
      expect(secondaryArea).toBeTruthy();

      expect(primaryLine.getAttribute('stroke')).toBe('#10b981');
      expect(secondaryLine.getAttribute('stroke')).toBe('#06b6d4');

      expect(primaryArea.getAttribute('fill')).toMatch(/url\(#.*-primary-gradient\)/);
      expect(secondaryArea.getAttribute('fill')).toMatch(/url\(#.*-secondary-gradient\)/);

      // Verify defs gradients exist
      const gradients = fixture.nativeElement.querySelectorAll('defs linearGradient');
      expect(gradients.length).toBe(2);
    });

    it('should not render secondary paths when secondarySamples is undefined', () => {
      const primary: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 50 },
      ];

      fixture.componentRef.setInput('samples', primary);
      fixture.componentRef.setInput('secondarySamples', undefined);
      fixture.detectChanges();

      const secondaryLine = fixture.nativeElement.querySelector('.secondary-line');
      const secondaryArea = fixture.nativeElement.querySelector('.secondary-area');

      expect(secondaryLine).toBeNull();
      expect(secondaryArea).toBeNull();
    });
  });

  describe('test case (d): gap preservation (null values) by splitting paths into disjoint segments', () => {
    it('should split line and area into disjoint segments around null values without bridging', () => {
      const samplesWithGap: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 10 },
        { timestamp: '2026-10-08T12:00:01Z', value: 20 },
        { timestamp: '2026-10-08T12:00:02Z', value: null }, // Gap
        { timestamp: '2026-10-08T12:00:03Z', value: 20 },
        { timestamp: '2026-10-08T12:00:04Z', value: 30 },
      ];

      fixture.componentRef.setInput('samples', samplesWithGap);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const areaEl = fixture.nativeElement.querySelector('.primary-area');

      const lineD = lineEl.getAttribute('d');
      const areaD = areaEl.getAttribute('d');

      expect(lineD).toBeTruthy();
      expect(lineD).not.toContain('NaN');

      // Must have 2 distinct M commands for the 2 segments
      const lineM = lineD.match(/M/g);
      expect(lineM?.length).toBe(2);

      // 5 points: x = 0, 75, 150 (gap), 225, 300
      // min=10, max=30, range=20
      // Segment 1: M 0 76 L 75 40
      // Segment 2: M 225 40 L 300 4
      expect(lineD).toBe('M 0 76 L 75 40 M 225 40 L 300 4');

      // Area must have 2 distinct Z commands (closing each segment to baseline independently)
      const areaZ = areaD.match(/Z/g);
      expect(areaZ?.length).toBe(2);
      expect(areaD).toBe(
        'M 0 76 L 75 40 L 75 76 L 0 76 Z M 225 40 L 300 4 L 300 76 L 225 76 Z'
      );
    });

    it('should handle multiple disjoint gaps cleanly', () => {
      const samplesMultipleGaps: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 10 },
        { timestamp: '2026-10-08T12:00:01Z', value: null },
        { timestamp: '2026-10-08T12:00:02Z', value: 20 },
        { timestamp: '2026-10-08T12:00:03Z', value: null },
        { timestamp: '2026-10-08T12:00:04Z', value: 30 },
      ];

      fixture.componentRef.setInput('samples', samplesMultipleGaps);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const areaEl = fixture.nativeElement.querySelector('.primary-area');

      const lineD = lineEl.getAttribute('d');
      const areaD = areaEl.getAttribute('d');

      const lineM = lineD.match(/M/g);
      expect(lineM?.length).toBe(3);

      const areaZ = areaD.match(/Z/g);
      expect(areaZ?.length).toBe(3);
    });

    it('should handle leading and trailing nulls without broken path commands', () => {
      const samplesEdgeNulls: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: null },
        { timestamp: '2026-10-08T12:00:01Z', value: 10 },
        { timestamp: '2026-10-08T12:00:02Z', value: 20 },
        { timestamp: '2026-10-08T12:00:03Z', value: null },
      ];

      fixture.componentRef.setInput('samples', samplesEdgeNulls);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const lineD = lineEl.getAttribute('d');

      const lineM = lineD.match(/M/g);
      expect(lineM?.length).toBe(1);
      // N=4: x = (1/3)*300 = 100, x = (2/3)*300 = 200
      expect(lineD).toBe('M 100 76 L 200 4');
    });
  });

  describe('test case (e): respects maxValue input when scaling Y coordinates', () => {
    it('should scale Y coordinates against maxValue when provided', () => {
      const samples: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 50 },
        { timestamp: '2026-10-08T12:00:01Z', value: 50 },
      ];

      // Without maxValue: min=50, max=50 -> baseline at 76
      fixture.componentRef.setInput('samples', samples);
      fixture.componentRef.setInput('maxValue', undefined);
      fixture.detectChanges();

      const lineElDefault = fixture.nativeElement.querySelector('.primary-line');
      expect(lineElDefault.getAttribute('d')).toBe('M 0 76 L 300 76');

      // With maxValue = 100: min=0, max=100, range=100.
      // val 50 -> y = 80 - 4 - (50/100) * 72 = 76 - 36 = 40 (mid-height)
      fixture.componentRef.setInput('maxValue', 100);
      fixture.detectChanges();

      const lineElScaled = fixture.nativeElement.querySelector('.primary-line');
      expect(lineElScaled.getAttribute('d')).toBe('M 0 40 L 300 40');
    });

    it('should handle shared scale between primary and secondary traces with maxValue', () => {
      const primary: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 25 },
      ];
      const secondary: TelemetrySample<number | null>[] = [
        { timestamp: '2026-10-08T12:00:00Z', value: 75 },
      ];

      fixture.componentRef.setInput('samples', primary);
      fixture.componentRef.setInput('secondarySamples', secondary);
      fixture.componentRef.setInput('maxValue', 100);
      fixture.detectChanges();

      const primaryLine = fixture.nativeElement.querySelector('.primary-line');
      const secondaryLine = fixture.nativeElement.querySelector('.secondary-line');

      // 25% -> y = 80 - 4 - 0.25 * 72 = 76 - 18 = 58
      // 75% -> y = 80 - 4 - 0.75 * 72 = 76 - 54 = 22
      expect(primaryLine.getAttribute('d')).toBe('M 0 58');
      expect(secondaryLine.getAttribute('d')).toBe('M 0 22');
    });
  });

  describe('test case (f): renders legend labels and swatches when provided', () => {
    it('should render legend bar with primary and secondary labels and swatches', () => {
      fixture.componentRef.setInput('primaryLabel', 'Inbound (RX)');
      fixture.componentRef.setInput('secondaryLabel', 'Outbound (TX)');
      fixture.componentRef.setInput('primaryColor', '#34d399');
      fixture.componentRef.setInput('secondaryColor', '#38bdf8');
      fixture.componentRef.setInput('unit', 'KB/s');
      fixture.detectChanges();

      const legend = fixture.nativeElement.querySelector('[data-testid="chart-legend"]');
      expect(legend).toBeTruthy();

      const primaryLegend = fixture.nativeElement.querySelector('[data-testid="primary-legend"]');
      const secondaryLegend = fixture.nativeElement.querySelector('[data-testid="secondary-legend"]');
      const unitEl = fixture.nativeElement.querySelector('[data-testid="chart-unit"]');

      expect(primaryLegend).toBeTruthy();
      expect(primaryLegend.textContent).toContain('Inbound (RX)');

      expect(secondaryLegend).toBeTruthy();
      expect(secondaryLegend.textContent).toContain('Outbound (TX)');

      expect(unitEl).toBeTruthy();
      expect(unitEl.textContent).toContain('KB/s');
    });

    it('should render legend with only primary label when secondaryLabel is omitted', () => {
      fixture.componentRef.setInput('primaryLabel', 'CPU Usage');
      fixture.componentRef.setInput('secondaryLabel', undefined);
      fixture.detectChanges();

      const legend = fixture.nativeElement.querySelector('[data-testid="chart-legend"]');
      expect(legend).toBeTruthy();

      const primaryLegend = fixture.nativeElement.querySelector('[data-testid="primary-legend"]');
      const secondaryLegend = fixture.nativeElement.querySelector('[data-testid="secondary-legend"]');

      expect(primaryLegend).toBeTruthy();
      expect(primaryLegend.textContent).toContain('CPU Usage');
      expect(secondaryLegend).toBeNull();
    });
  });

  describe('sample window bound (last 60 samples)', () => {
    it('should cap buffer to last 60 samples when more are provided', () => {
      const seventySamples: TelemetrySample<number | null>[] = Array.from(
        { length: 70 },
        (_, i) => ({
          timestamp: `2026-10-08T12:00:${i.toString().padStart(2, '0')}Z`,
          value: i < 10 ? 999 : 10, // First 10 samples have value 999, remaining 60 have 10
        })
      );

      fixture.componentRef.setInput('samples', seventySamples);
      fixture.detectChanges();

      const lineEl = fixture.nativeElement.querySelector('.primary-line');
      const d = lineEl.getAttribute('d');

      // Since only the last 60 are used, all values are 10 (not 999)
      // Extract every Y value (every second number)
      const numbers = (d?.replace(/[ML] /g, '').split(' ') ?? []).map(Number);
      const yValues = numbers.filter((_: number, idx: number) => idx % 2 === 1);
      expect(yValues.length).toBe(60);
      expect(yValues.every((y: number) => y === 76)).toBe(true);
    });
  });

  describe('custom dimensions', () => {
    it('should update viewBox and scale paths when width and height change', () => {
      fixture.componentRef.setInput('width', 400);
      fixture.componentRef.setInput('height', 120);
      fixture.componentRef.setInput('samples', []);
      fixture.detectChanges();

      const svgEl = fixture.nativeElement.querySelector('svg');
      const lineEl = fixture.nativeElement.querySelector('.primary-line');

      expect(svgEl.getAttribute('viewBox')).toBe('0 0 400 120');
      // For h=120, baseline Y is 120 - 4 = 116
      expect(lineEl.getAttribute('d')).toBe('M 0 116 L 400 116');
    });
  });
});
