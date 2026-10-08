import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
} from '@angular/core';
import { HostStateService } from '../../../../core/services/host-state.service';
import { SseService } from '../../../../core/api/sse.service';
import { RollingAreaChartComponent } from '../../../../shared/charts/rolling-area-chart/rolling-area-chart.component';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { formatRate } from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-network-card',
  standalone: true,
  imports: [RollingAreaChartComponent, StatusBadgeComponent],
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 flex flex-col justify-between">
      <!-- Card Header -->
      <div class="flex items-center justify-between gap-2 border-b border-zinc-800/80 pb-3">
        <span class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
          NETWORK THROUGHPUT
        </span>
        <span class="font-mono text-xs text-zinc-400">
          HOST AGGREGATE (SSE)
        </span>
      </div>

      <!-- Headline Metric (Neutral monospace high-contrast text) -->
      <div class="my-3">
        <div class="flex items-baseline gap-4">
          <div>
            <span class="text-[11px] text-zinc-400 block uppercase">Ingress (RX)</span>
            <span class="font-mono text-2xl font-bold text-sky-400">
              {{ rxRate() }}
            </span>
          </div>
          <div>
            <span class="text-[11px] text-zinc-400 block uppercase">Egress (TX)</span>
            <span class="font-mono text-2xl font-bold text-emerald-400">
              {{ txRate() }}
            </span>
          </div>
        </div>
      </div>

      <!-- Dual Trace Rolling Chart -->
      <div class="my-2 border border-zinc-800/50 rounded bg-zinc-950/40 p-2">
        <np-rolling-area-chart
          [samples]="hostState.networkRxRollingBuffer()"
          [secondarySamples]="hostState.networkTxRollingBuffer()"
          primaryLabel="RX"
          secondaryLabel="TX"
          primaryColor="#38bdf8"
          secondaryColor="#34d399"
          unit="B/s"
          [height]="72"
        />
      </div>

      <!-- Interface Inventory & Hardware Error Counts -->
      <div class="pt-3 border-t border-zinc-800/80 space-y-2">
        <span class="text-[11px] text-zinc-400 block uppercase">
          Physical Interfaces
        </span>
        @if (interfaces().length === 0) {
          <span class="text-xs text-zinc-400 font-mono">No network telemetry</span>
        } @else {
          <div class="space-y-1.5 max-h-24 overflow-y-auto pr-1">
            @for (iface of interfaces(); track iface.name) {
              <div class="flex items-center justify-between text-xs py-1 border-b border-zinc-800/40 last:border-b-0">
                <div class="flex items-center gap-2">
                  <span class="font-mono font-semibold text-zinc-200">{{ iface.name }}</span>
                  <np-status-badge [status]="iface.operstate" [label]="iface.operstate" />
                  <span class="text-[11px] text-zinc-400">{{ iface.speed_mbps }} Mbps</span>
                </div>
                <div class="font-mono text-[11px] text-zinc-400">
                  Errors: {{ iface.rx_errors }} RX / {{ iface.tx_errors }} TX
                </div>
              </div>
            }
          </div>
        }
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class NetworkCardComponent {
  readonly hostState = inject(HostStateService);
  readonly sse = inject(SseService);

  readonly rxRate = computed(() => {
    const pulse = this.sse.pulse();
    if (!pulse) return '—';
    return formatRate(pulse.network_rx_bytes_sec);
  });

  readonly txRate = computed(() => {
    const pulse = this.sse.pulse();
    if (!pulse) return '—';
    return formatRate(pulse.network_tx_bytes_sec);
  });

  readonly interfaces = computed(
    () => this.hostState.networkMetrics() ?? [],
  );
}
