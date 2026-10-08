import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
} from '@angular/core';
import { Router } from '@angular/router';
import { HostStateService } from '../../../../core/services/host-state.service';
import { formatBytes } from '../../../../shared/utils/formatters';
import { ProcessInfo } from '../../../../core/models/process.model';

@Component({
  selector: 'np-top-processes',
  standalone: true,
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 sm:p-5">
      <!-- Section Header -->
      <div class="flex items-center justify-between gap-2 border-b border-zinc-800/80 pb-3 mb-3">
        <h2 class="text-sm font-semibold text-zinc-200 uppercase tracking-wider">
          Top Processes by CPU
        </h2>
        <span class="font-mono text-xs text-zinc-400">
          TOP 5
        </span>
      </div>

      <!-- Table Container -->
      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-zinc-800 text-zinc-400 font-medium uppercase tracking-wider">
              <th scope="col" class="py-2.5 px-3">PID</th>
              <th scope="col" class="py-2.5 px-3">Name</th>
              <th scope="col" class="py-2.5 px-3">User</th>
              <th scope="col" class="py-2.5 px-3 text-right">CPU %</th>
              <th scope="col" class="py-2.5 px-3 text-right">Memory (RSS)</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-zinc-800/60">
            @if (processes().length === 0) {
              <tr>
                <td colspan="5" class="py-6 text-center text-xs text-zinc-400 font-mono">
                  No process telemetry
                </td>
              </tr>
            } @else {
              @for (proc of processes(); track proc.pid) {
                <tr
                  tabindex="0"
                  role="button"
                  (click)="onRowClick(proc)"
                  (keydown.enter)="onRowClick(proc)"
                  (keydown.space)="onRowClick(proc); $event.preventDefault()"
                  class="hover:bg-zinc-800/50 cursor-pointer transition-colors focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
                >
                  <td class="py-2.5 px-3 font-mono text-zinc-300">
                    {{ proc.pid }}
                  </td>
                  <td class="py-2.5 px-3 font-medium text-zinc-100">
                    {{ proc.name }}
                  </td>
                  <td class="py-2.5 px-3 text-zinc-400">
                    {{ proc.user }}
                  </td>
                  <td class="py-2.5 px-3 text-right font-mono font-semibold text-zinc-100">
                    {{ proc.cpu_percent.toFixed(1) }}%
                  </td>
                  <td class="py-2.5 px-3 text-right font-mono text-zinc-300">
                    {{ formatBytes(proc.memory_rss_bytes) }}
                  </td>
                </tr>
              }
            }
          </tbody>
        </table>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class TopProcessesComponent {
  private readonly hostState = inject(HostStateService);
  private readonly router = inject(Router);

  readonly formatBytes = formatBytes;

  readonly processes = computed(() => {
    const list = this.hostState.topProcesses();
    if (!list) return [];
    return list.slice(0, 5);
  });

  onRowClick(proc: ProcessInfo): void {
    this.router.navigate(['/processes'], {
      queryParams: { pid: proc.pid },
    });
  }
}
