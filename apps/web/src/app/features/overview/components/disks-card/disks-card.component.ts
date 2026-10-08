import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
} from '@angular/core';
import { HostStateService } from '../../../../core/services/host-state.service';
import { formatBytes } from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-disks-card',
  standalone: true,
  template: `
    <div class="bg-zinc-900 border border-zinc-800 rounded p-4 flex flex-col justify-between">
      <!-- Card Header -->
      <div class="flex items-center justify-between gap-2 border-b border-zinc-800/80 pb-3">
        <span class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
          FILESYSTEM CAPACITY
        </span>
        <span class="font-mono text-xs text-zinc-400">
          MOUNT POINTS
        </span>
      </div>

      <!-- Partitions List -->
      <div class="my-3 space-y-3 flex-1 overflow-y-auto max-h-56 pr-1">
        @if (partitions().length === 0) {
          <div class="py-6 text-center text-xs text-zinc-400 font-mono">
            No filesystem telemetry
          </div>
        } @else {
          @for (part of partitions(); track part.mount_point) {
            <div class="border border-zinc-800/60 rounded p-2.5 bg-zinc-950/40 space-y-1.5">
              <div class="flex items-center justify-between text-xs">
                <div class="flex items-center gap-2 truncate">
                  <span class="font-mono font-bold text-zinc-100 text-sm">
                    {{ part.mount_point }}
                  </span>
                  <span class="text-[11px] text-zinc-400 font-mono">
                    {{ part.fstype }}
                  </span>
                  <span class="text-[11px] text-zinc-400 truncate max-w-[120px]" [title]="part.filesystem">
                    ({{ part.filesystem }})
                  </span>
                </div>
                <!-- Neutral monospace percentage -->
                <span class="font-mono text-xs font-semibold text-zinc-100 shrink-0">
                  {{ part.usage_percent.toFixed(1) }}%
                </span>
              </div>

              <!-- Capacity Progress Bar (neutral styling, zero arbitrary threshold colors) -->
              <div class="w-full bg-zinc-800 rounded-full h-1.5 overflow-hidden">
                <div
                  class="bg-zinc-400 h-1.5 rounded-full"
                  [style.width.%]="Math.min(part.usage_percent, 100)"
                ></div>
              </div>

              <div class="flex items-center justify-between text-[11px] text-zinc-400 font-mono">
                <span>
                  {{ formatBytes(part.used_bytes) }} / {{ formatBytes(part.total_bytes) }}
                </span>
                <span>
                  Avail: {{ formatBytes(part.available_bytes) }}
                </span>
              </div>

              @if (part.inodes_total > 0) {
                <div class="text-[10px] text-zinc-500 font-mono flex items-center justify-between pt-0.5">
                  <span>Inodes</span>
                  <span>{{ (part.inodes_total - part.inodes_free).toLocaleString() }} / {{ part.inodes_total.toLocaleString() }}</span>
                </div>
              }
            </div>
          }
        }
      </div>

      <!-- Root Filesystem Summary Footer -->
      <div class="pt-3 border-t border-zinc-800/80 text-xs text-zinc-400 flex items-center justify-between">
        <span>Partitions Tracked</span>
        <span class="font-mono text-zinc-200 font-semibold">{{ partitions().length }}</span>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class DisksCardComponent {
  readonly hostState = inject(HostStateService);

  readonly Math = Math;
  readonly formatBytes = formatBytes;

  readonly partitions = computed(
    () => this.hostState.diskMetrics() ?? [],
  );
}
