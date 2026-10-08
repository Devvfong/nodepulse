import { ChangeDetectionStrategy, Component, computed, input } from '@angular/core';

@Component({
  selector: 'np-sparkline',
  standalone: true,
  template: `
    <svg
      [attr.viewBox]="viewBox()"
      [attr.width]="width()"
      [attr.height]="height()"
      class="overflow-visible block"
      fill="none"
      aria-hidden="true"
    >
      <path
        [attr.d]="pathD()"
        [attr.stroke]="strokeColor()"
        [attr.stroke-width]="strokeWidth()"
        stroke-linecap="round"
        stroke-linejoin="round"
        fill="none"
      />
    </svg>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class SparklineComponent {
  readonly data = input<(number | null)[]>([]);
  readonly width = input<number>(120);
  readonly height = input<number>(32);
  readonly strokeColor = input<string>('#34d399');
  readonly strokeWidth = input<number>(1.5);

  readonly viewBox = computed(() => `0 0 ${this.width()} ${this.height()}`);

  readonly pathD = computed(() => {
    const rawData = this.data();
    const w = this.width();
    const h = this.height();

    const validNumbers = rawData.filter(
      (v): v is number => v !== null && v !== undefined && !Number.isNaN(v)
    );

    if (validNumbers.length === 0) {
      const baselineY = Math.round((h / 2) * 100) / 100;
      return `M 0 ${baselineY} L ${w} ${baselineY}`;
    }

    const min = Math.min(...validNumbers);
    const max = Math.max(...validNumbers);
    const range = Math.max(max - min, 1);
    const availableHeight = Math.max(h - 4, 0);

    const segments: string[] = [];
    let currentSegment: string[] = [];

    for (let i = 0; i < rawData.length; i++) {
      const val = rawData[i];
      if (val === null || val === undefined || Number.isNaN(val)) {
        if (currentSegment.length > 0) {
          segments.push(currentSegment.join(' '));
          currentSegment = [];
        }
      } else {
        const x = rawData.length > 1 ? (i / (rawData.length - 1)) * w : 0;
        const y = h - ((val - min) / range) * availableHeight - 2;

        const formattedX = Math.round(x * 100) / 100;
        const formattedY = Math.round(y * 100) / 100;

        if (currentSegment.length === 0) {
          currentSegment.push(`M ${formattedX} ${formattedY}`);
        } else {
          currentSegment.push(`L ${formattedX} ${formattedY}`);
        }
      }
    }

    if (currentSegment.length > 0) {
      segments.push(currentSegment.join(' '));
    }

    return segments.join(' ');
  });
}
