import { ChangeDetectionStrategy, Component, computed, input } from '@angular/core';
import { StatusBadgeComponent } from '../status-badge/status-badge.component';

@Component({
  selector: 'np-metric-card',
  standalone: true,
  imports: [StatusBadgeComponent],
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 flex flex-col justify-between">
      <div>
        <div class="flex items-center justify-between gap-2 mb-2">
          <span class="metric-label text-zinc-400 text-xs font-medium uppercase tracking-wide">
            {{ label() }}
          </span>
          @if (status()) {
            <np-status-badge [status]="status()!" />
          }
        </div>

        <div class="flex items-baseline gap-1">
          <span class="metric-value font-mono text-2xl font-bold text-zinc-100">
            {{ displayValue() }}
          </span>
          @if (unit() && hasValue()) {
            <span class="metric-unit text-zinc-400 text-sm font-normal">{{ unit() }}</span>
          }
        </div>

        @if (subtext()) {
          <div class="metric-subtext text-zinc-400 text-xs font-mono mt-1">
            {{ subtext() }}
          </div>
        }
      </div>

      <ng-content></ng-content>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class MetricCardComponent {
  readonly label = input<string>('');
  readonly value = input<string | number | null>(null);
  readonly unit = input<string | undefined>(undefined);
  readonly subtext = input<string | undefined>(undefined);
  readonly status = input<string | undefined>(undefined);

  readonly hasValue = computed<boolean>(() => {
    const v = this.value();
    return v !== null && v !== undefined;
  });

  readonly displayValue = computed<string>(() => {
    const v = this.value();
    if (v === null || v === undefined) {
      return '—';
    }
    return String(v);
  });
}
