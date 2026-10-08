import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
} from '@angular/core';
import { HostStateService } from '../../../../core/services/host-state.service';
import { RollingAreaChartComponent } from '../../../../shared/charts/rolling-area-chart/rolling-area-chart.component';
import { formatBytes } from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-memory-card',
  standalone: true,
  imports: [RollingAreaChartComponent],
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 flex flex-col justify-between">
      <!-- Card Header -->
      <div class="flex items-center justify-between gap-2 border-b border-zinc-800/80 pb-3">
        <span class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
          MEMORY USAGE
        </span>
        <span class="font-mono text-xs text-zinc-400">
          RAM & SWAP
        </span>
      </div>

      <!-- Headline Metric (Neutral monospace high-contrast text) -->
      <div class="my-3">
        <div class="flex items-baseline gap-2">
          <span class="font-mono text-3xl font-bold text-zinc-100">
            {{ displayPercentage() }}
          </span>
          <span class="text-xs font-mono text-zinc-400">
            {{ usedTotal() }}
          </span>
        </div>
        <p class="text-xs text-zinc-400 truncate mt-0.5">
          Buffers: {{ buffers() }} • Cached: {{ cached() }}
        </p>
      </div>

      <!-- Rolling Chart -->
      <div class="my-2 border border-zinc-800/50 rounded bg-zinc-950/40 p-2">
        <np-rolling-area-chart
          [samples]="hostState.memoryRollingBuffer()"
          [maxValue]="100"
          [unit]="'%'"
          primaryLabel="RAM"
          primaryColor="#a78bfa"
          [height]="72"
        />
      </div>

      <!-- Subsystem Details (Swap Status) -->
      <div class="pt-3 border-t border-zinc-800/80 flex items-center justify-between text-xs">
        <div>
          <span class="text-[11px] text-zinc-400 block uppercase">Swap Usage</span>
          <span class="font-mono text-xs font-semibold text-zinc-200 mt-0.5 block">
            {{ swapUsage() }}
          </span>
        </div>
        <div class="text-right">
          <span class="text-[11px] text-zinc-400 block uppercase">Swap Allocation</span>
          <span class="font-mono text-xs font-semibold text-zinc-200 mt-0.5 block">
            {{ swapAllocation() }}
          </span>
        </div>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class MemoryCardComponent {
  readonly hostState = inject(HostStateService);

  readonly displayPercentage = computed(() => {
    const mem = this.hostState.memoryMetrics();
    if (!mem) return '—';
    return `${mem.usage_percent.toFixed(1)}%`;
  });

  readonly usedTotal = computed(() => {
    const mem = this.hostState.memoryMetrics();
    if (!mem) return '—';
    return `${formatBytes(mem.used_bytes)} / ${formatBytes(mem.total_bytes)}`;
  });

  readonly buffers = computed(() =>
    formatBytes(this.hostState.memoryMetrics()?.buffers_bytes),
  );

  readonly cached = computed(() =>
    formatBytes(this.hostState.memoryMetrics()?.cached_bytes),
  );

  readonly swapUsage = computed(() => {
    const mem = this.hostState.memoryMetrics();
    if (!mem) return '—';
    return `${mem.swap_usage_percent.toFixed(1)}%`;
  });

  readonly swapAllocation = computed(() => {
    const mem = this.hostState.memoryMetrics();
    if (!mem) return '—';
    return `${formatBytes(mem.swap_used_bytes)} / ${formatBytes(mem.swap_total_bytes)}`;
  });
}
