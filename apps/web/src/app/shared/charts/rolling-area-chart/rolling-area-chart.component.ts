import {
  ChangeDetectionStrategy,
  Component,
  computed,
  input,
} from '@angular/core';
import { TelemetrySample } from '../../../core/models/pulse.model';

interface SegmentPoint {
  x: number;
  y: number;
}

@Component({
  selector: 'np-rolling-area-chart',
  standalone: true,
  template: `
    @if (hasLegend()) {
      <div
        class="flex items-center justify-between text-xs font-mono text-slate-400 mb-1.5"
        data-testid="chart-legend"
      >
        <div class="flex items-center gap-4">
          @if (primaryLabel()) {
            <div class="flex items-center gap-1.5" data-testid="primary-legend">
              <span
                class="inline-block w-2.5 h-2.5 rounded-sm"
                [style.backgroundColor]="primaryColor()"
              ></span>
              <span>{{ primaryLabel() }}</span>
            </div>
          }
          @if (secondaryLabel()) {
            <div class="flex items-center gap-1.5" data-testid="secondary-legend">
              <span
                class="inline-block w-2.5 h-2.5 rounded-sm"
                [style.backgroundColor]="secondaryColor()"
              ></span>
              <span>{{ secondaryLabel() }}</span>
            </div>
          }
        </div>
        @if (unit()) {
          <span class="text-slate-500" data-testid="chart-unit">{{ unit() }}</span>
        }
      </div>
    }
    <svg
      [attr.viewBox]="viewBox()"
      class="w-full h-auto overflow-visible block"
      preserveAspectRatio="none"
      aria-hidden="true"
    >
      <defs>
        <linearGradient [attr.id]="primaryGradientId()" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" [attr.stop-color]="primaryColor()" stop-opacity="0.25" />
          <stop offset="100%" [attr.stop-color]="primaryColor()" stop-opacity="0.02" />
        </linearGradient>
        @if (secondarySamples()) {
          <linearGradient [attr.id]="secondaryGradientId()" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" [attr.stop-color]="secondaryColor()" stop-opacity="0.25" />
            <stop offset="100%" [attr.stop-color]="secondaryColor()" stop-opacity="0.02" />
          </linearGradient>
        }
      </defs>

      @if (secondaryAreaD()) {
        <path
          [attr.d]="secondaryAreaD()"
          [attr.fill]="'url(#' + secondaryGradientId() + ')'"
          class="secondary-area"
        />
      }
      @if (primaryAreaD()) {
        <path
          [attr.d]="primaryAreaD()"
          [attr.fill]="'url(#' + primaryGradientId() + ')'"
          class="primary-area"
        />
      }

      @if (secondaryLineD()) {
        <path
          [attr.d]="secondaryLineD()"
          [attr.stroke]="secondaryColor()"
          stroke-width="1.5"
          fill="none"
          stroke-linecap="round"
          stroke-linejoin="round"
          class="secondary-line"
        />
      }
      <path
        [attr.d]="primaryLineD()"
        [attr.stroke]="primaryColor()"
        stroke-width="1.5"
        fill="none"
        stroke-linecap="round"
        stroke-linejoin="round"
        class="primary-line"
      />
    </svg>
  `,
  host: {
    class: 'block w-full',
  },
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class RollingAreaChartComponent {
  private static instanceCounter = 0;
  private readonly instanceId = `np-rac-${++RollingAreaChartComponent.instanceCounter}`;

  readonly samples = input<TelemetrySample<number | null>[]>([]);
  readonly secondarySamples = input<TelemetrySample<number | null>[] | undefined>(undefined);
  readonly primaryLabel = input<string>('');
  readonly secondaryLabel = input<string | undefined>(undefined);
  readonly primaryColor = input<string>('#34d399');
  readonly secondaryColor = input<string>('#38bdf8');
  readonly unit = input<string>('');
  readonly maxValue = input<number | undefined>(undefined);
  readonly height = input<number>(80);
  readonly width = input<number>(300);

  readonly primaryGradientId = computed(() => `${this.instanceId}-primary-gradient`);
  readonly secondaryGradientId = computed(() => `${this.instanceId}-secondary-gradient`);

  readonly viewBox = computed(() => `0 0 ${this.width()} ${this.height()}`);
  readonly hasLegend = computed(() => Boolean(this.primaryLabel() || this.secondaryLabel()));

  private readonly chartGeometry = computed(() => {
    const w = this.width();
    const h = this.height();
    const padding = 4;
    const availableHeight = Math.max(h - 2 * padding, 0);
    const baselineY = Math.round((h - padding) * 100) / 100;

    const primaryData = this.samples().slice(-60);
    const secondaryData = this.secondarySamples()?.slice(-60);

    const extractValid = (data?: TelemetrySample<number | null>[]): number[] => {
      if (!data) return [];
      return data
        .map((s) => s?.value)
        .filter((v): v is number => v !== null && v !== undefined && !Number.isNaN(v));
    };

    const primaryValid = extractValid(primaryData);
    const secondaryValid = extractValid(secondaryData);
    const allValid = [...primaryValid, ...secondaryValid];

    const maxInput = this.maxValue();
    let min: number;
    let max: number;

    if (maxInput !== undefined) {
      min = 0;
      max = Math.max(maxInput, ...(allValid.length > 0 ? allValid : [0]));
    } else if (allValid.length > 0) {
      min = Math.min(...allValid);
      max = Math.max(...allValid);
    } else {
      min = 0;
      max = 1;
    }

    const range = Math.max(max - min, 1);

    const generateSegments = (data: TelemetrySample<number | null>[]): SegmentPoint[][] => {
      const n = data.length;
      const segments: SegmentPoint[][] = [];
      let current: SegmentPoint[] = [];

      for (let i = 0; i < n; i++) {
        const val = data[i]?.value;
        if (val === null || val === undefined || Number.isNaN(val)) {
          if (current.length > 0) {
            segments.push(current);
            current = [];
          }
        } else {
          const x = n > 1 ? (i / (n - 1)) * w : 0;
          const y = h - padding - ((val - min) / range) * availableHeight;
          current.push({
            x: Math.round(x * 100) / 100,
            y: Math.round(y * 100) / 100,
          });
        }
      }

      if (current.length > 0) {
        segments.push(current);
      }
      return segments;
    };

    const buildPaths = (
      segments: SegmentPoint[][],
      isPrimary: boolean
    ): { lineD: string; areaD: string } => {
      if (segments.length === 0) {
        return {
          lineD: isPrimary ? `M 0 ${baselineY} L ${w} ${baselineY}` : '',
          areaD: '',
        };
      }

      const lineSegments: string[] = [];
      const areaSegments: string[] = [];

      for (const segment of segments) {
        const lineCmds: string[] = [];
        lineCmds.push(`M ${segment[0].x} ${segment[0].y}`);
        for (let j = 1; j < segment.length; j++) {
          lineCmds.push(`L ${segment[j].x} ${segment[j].y}`);
        }
        lineSegments.push(lineCmds.join(' '));

        const firstX = segment[0].x;
        const lastX = segment[segment.length - 1].x;
        areaSegments.push(
          `${lineCmds.join(' ')} L ${lastX} ${baselineY} L ${firstX} ${baselineY} Z`
        );
      }

      return {
        lineD: lineSegments.join(' '),
        areaD: areaSegments.join(' '),
      };
    };

    const primaryPaths = buildPaths(generateSegments(primaryData), true);
    const secondaryPaths = secondaryData
      ? buildPaths(generateSegments(secondaryData), false)
      : { lineD: '', areaD: '' };

    return {
      primaryLineD: primaryPaths.lineD,
      primaryAreaD: primaryPaths.areaD,
      secondaryLineD: secondaryPaths.lineD,
      secondaryAreaD: secondaryPaths.areaD,
    };
  });

  readonly primaryLineD = computed(() => this.chartGeometry().primaryLineD);
  readonly primaryAreaD = computed(() => this.chartGeometry().primaryAreaD);
  readonly secondaryLineD = computed(() => this.chartGeometry().secondaryLineD);
  readonly secondaryAreaD = computed(() => this.chartGeometry().secondaryAreaD);
}
