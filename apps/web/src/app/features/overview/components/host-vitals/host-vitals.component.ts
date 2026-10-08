import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
} from '@angular/core';
import { HostStateService } from '../../../../core/services/host-state.service';
import { ConnectionStateService } from '../../../../core/services/connection-state.service';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import {
  formatEpochSeconds,
  formatUptime,
} from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-host-vitals',
  standalone: true,
  imports: [StatusBadgeComponent],
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 sm:p-5">
      <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4 border-b border-zinc-800/80 pb-4">
        <!-- Hostname & Identity -->
        <div>
          <div class="flex items-center gap-2.5">
            <h1 class="font-mono text-xl sm:text-2xl font-bold text-zinc-100 tracking-tight">
              {{ hostname() }}
            </h1>
            <span class="text-xs font-mono text-zinc-400 bg-zinc-800 px-2 py-0.5 rounded">
              {{ architecture() }}
            </span>
          </div>
          <p class="text-xs text-zinc-400 mt-1">
            {{ osInfo() }} • Kernel {{ kernelVersion() }}
          </p>
        </div>

        <!-- Decoupled Status Badges -->
        <div class="flex items-center gap-2 shrink-0">
          <np-status-badge
            [status]="connectionState.agentStatus()"
            [label]="'AGENT: ' + connectionState.agentStatus().toUpperCase()"
          />
          <np-status-badge
            [status]="connectionState.streamStatus()"
            [label]="'STREAM: ' + connectionState.streamStatus().toUpperCase()"
          />
        </div>
      </div>

      <!-- Secondary Vitals Row -->
      <div class="grid grid-cols-2 sm:grid-cols-4 gap-4 mt-4 pt-1">
        <div>
          <span class="text-xs text-zinc-400 uppercase tracking-wider block">Uptime</span>
          <span class="font-mono text-sm font-semibold text-zinc-200 mt-0.5 block">
            {{ uptime() }}
          </span>
        </div>

        <div>
          <span class="text-xs text-zinc-400 uppercase tracking-wider block">Boot Time</span>
          <span class="font-mono text-sm font-semibold text-zinc-200 mt-0.5 block" [title]="bootTime()">
            {{ bootTime() }}
          </span>
        </div>

        <div>
          <span class="text-xs text-zinc-400 uppercase tracking-wider block">OS Platform</span>
          <span class="font-mono text-sm font-semibold text-zinc-200 mt-0.5 block truncate">
            {{ osInfo() }}
          </span>
        </div>

        <div>
          <span class="text-xs text-zinc-400 uppercase tracking-wider block">Architecture</span>
          <span class="font-mono text-sm font-semibold text-zinc-200 mt-0.5 block">
            {{ architecture() }}
          </span>
        </div>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class HostVitalsComponent {
  readonly hostState = inject(HostStateService);
  readonly connectionState = inject(ConnectionStateService);

  readonly hostname = computed(
    () => this.hostState.systemInfo()?.hostname ?? 'localhost',
  );

  readonly architecture = computed(
    () => this.hostState.systemInfo()?.architecture ?? '—',
  );

  readonly osInfo = computed(() => {
    const sys = this.hostState.systemInfo();
    if (!sys) return '—';
    return `${sys.os_name} ${sys.os_version}`.trim() || '—';
  });

  readonly kernelVersion = computed(
    () => this.hostState.systemInfo()?.kernel_version ?? '—',
  );

  readonly uptime = computed(() =>
    formatUptime(this.hostState.systemInfo()?.uptime_seconds),
  );

  readonly bootTime = computed(() =>
    formatEpochSeconds(this.hostState.systemInfo()?.boot_time_utc),
  );
}
