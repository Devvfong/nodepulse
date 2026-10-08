import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
} from '@angular/core';
import { HostStateService } from '../../../../core/services/host-state.service';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { RollingAreaChartComponent } from '../../../../shared/charts/rolling-area-chart/rolling-area-chart.component';

@Component({
  selector: 'np-cpu-card',
  standalone: true,
  imports: [StatusBadgeComponent, RollingAreaChartComponent],
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 flex flex-col justify-between">
      <!-- Card Header -->
      <div class="flex items-center justify-between gap-2 border-b border-zinc-800/80 pb-3">
        <span class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
          CPU USAGE
        </span>
        @if (statusBadge()) {
          <np-status-badge
            [status]="statusBadge()!.status"
            [label]="statusBadge()!.label"
          />
        }
      </div>

      <!-- Headline Metric (Neutral monospace high-contrast text) -->
      <div class="my-3">
        <div class="flex items-baseline gap-2">
          <span class="font-mono text-3xl font-bold text-zinc-100">
            {{ displayPercentage() }}
          </span>
          <span class="text-xs font-mono text-zinc-400">
            {{ coresInfo() }}
          </span>
        </div>
        <p class="text-xs text-zinc-400 truncate mt-0.5" [title]="cpuModel()">
          {{ cpuModel() }}
        </p>
      </div>

      <!-- Rolling Chart -->
      <div class="my-2 border border-zinc-800/50 rounded bg-zinc-950/40 p-2">
        <np-rolling-area-chart
          [samples]="hostState.cpuRollingBuffer()"
          [maxValue]="100"
          [unit]="'%'"
          primaryLabel="CPU"
          primaryColor="#34d399"
          [height]="72"
        />
      </div>

      <!-- Subsystem Details (Load Averages) -->
      <div class="pt-3 border-t border-zinc-800/80 grid grid-cols-3 gap-2 text-center">
        <div>
          <span class="text-[11px] text-zinc-400 block uppercase">1m Load</span>
          <span class="font-mono text-xs font-semibold text-zinc-200 mt-0.5 block">
            {{ load1m() }}
          </span>
        </div>
        <div>
          <span class="text-[11px] text-zinc-400 block uppercase">5m Load</span>
          <span class="font-mono text-xs font-semibold text-zinc-200 mt-0.5 block">
            {{ load5m() }}
          </span>
        </div>
        <div>
          <span class="text-[11px] text-zinc-400 block uppercase">15m Load</span>
          <span class="font-mono text-xs font-semibold text-zinc-200 mt-0.5 block">
            {{ load15m() }}
          </span>
        </div>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class CpuCardComponent {
  readonly hostState = inject(HostStateService);

  readonly statusBadge = computed(() => {
    const cpu = this.hostState.cpuMetrics();
    if (!cpu) return null;
    if (cpu.usage_percent === null && cpu.measurement_status === 'warming_up') {
      return { status: 'warming_up', label: 'WARMING UP' };
    }
    return { status: cpu.measurement_status, label: cpu.measurement_status.toUpperCase() };
  });

  readonly displayPercentage = computed(() => {
    const cpu = this.hostState.cpuMetrics();
    if (!cpu || cpu.usage_percent === null) return '—';
    return `${cpu.usage_percent.toFixed(1)}%`;
  });

  readonly cpuModel = computed(
    () => this.hostState.cpuMetrics()?.model_name || '—',
  );

  readonly coresInfo = computed(() => {
    const cpu = this.hostState.cpuMetrics();
    if (!cpu) return '';
    return `${cpu.physical_cores} physical / ${cpu.logical_cores} logical`;
  });

  readonly load1m = computed(() => {
    const l = this.hostState.cpuMetrics()?.load_average;
    return l ? l.one_minute.toFixed(2) : '—';
  });

  readonly load5m = computed(() => {
    const l = this.hostState.cpuMetrics()?.load_average;
    return l ? l.five_minute.toFixed(2) : '—';
  });

  readonly load15m = computed(() => {
    const l = this.hostState.cpuMetrics()?.load_average;
    return l ? l.fifteen_minute.toFixed(2) : '—';
  });
}
