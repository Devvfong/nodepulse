import {
  ChangeDetectionStrategy,
  Component,
  computed,
  DestroyRef,
  effect,
  inject,
  input,
  output,
  signal,
  untracked,
} from '@angular/core';
import { Subscription } from 'rxjs';
import { ApiClientService } from '../../../../core/api/api-client.service';
import { ProcessDetail } from '../../../../core/models/process.model';
import { DrawerComponent } from '../../../../shared/ui/drawer/drawer.component';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { EmptyStateComponent } from '../../../../shared/ui/empty-state/empty-state.component';
import { formatBytes, formatDurationSince, formatEpochSeconds } from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-process-detail-drawer',
  standalone: true,
  imports: [DrawerComponent, StatusBadgeComponent, EmptyStateComponent],
  template: `
    <np-drawer
      [isOpen]="isOpen()"
      [title]="title()"
      [subtitle]="subtitle()"
      (close)="onClose()"
    >
      @if (detail(); as d) {
        <div drawer-header class="flex items-center gap-2">
          <np-status-badge [status]="getStateStatus(d.state)" [label]="d.state" />
          <span class="font-mono text-xs px-2 py-0.5 rounded-sm bg-zinc-800 text-zinc-300 border border-zinc-700">
            PID {{ d.pid }}
          </span>
        </div>
      }

      @if (detail(); as d) {

        <!-- Core Telemetry -->
        <section class="space-y-2">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Core Telemetry</h3>
          <div class="grid grid-cols-2 gap-2">
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <div class="text-xs text-zinc-400">CPU Usage</div>
              <div class="text-sm font-mono font-semibold text-zinc-100 mt-1">{{ d.cpu_percent.toFixed(1) }}%</div>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <div class="text-xs text-zinc-400">Memory (RSS)</div>
              <div class="text-sm font-mono font-semibold text-zinc-100 mt-1">{{ formatBytes(d.memory_rss_bytes) }}</div>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <div class="text-xs text-zinc-400">Virtual Memory (VMS)</div>
              <div class="text-sm font-mono font-semibold text-zinc-100 mt-1">{{ formatBytes(d.memory_vms_bytes) }}</div>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <div class="text-xs text-zinc-400">Threads</div>
              <div class="text-sm font-mono font-semibold text-zinc-100 mt-1">{{ d.thread_count }}</div>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5 col-span-2">
              <div class="text-xs text-zinc-400">Open File Descriptors</div>
              <div class="text-sm font-mono font-semibold text-zinc-100 mt-1">{{ d.open_fd_count }}</div>
            </div>
          </div>
        </section>

        <!-- Process Metadata -->
        <section class="space-y-2">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Process Metadata</h3>
          <dl class="grid grid-cols-2 gap-2 text-xs">
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Parent PID (PPID)</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ d.ppid }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">User</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ d.user }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5 col-span-2">
              <dt class="text-zinc-400">Start Time</dt>
              <dd class="font-mono text-zinc-200 mt-1">
                {{ formatEpochSeconds(d.start_time_epoch) }} ({{ formatDurationSince(d.start_time_epoch) }} ago)
              </dd>
            </div>
          </dl>
        </section>

        <!-- Working Directory -->
        <section class="space-y-1.5">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Working Directory</h3>
          <div class="bg-zinc-950 border border-zinc-800 rounded p-2.5 text-xs font-mono text-zinc-300 break-all select-all">
            {{ d.working_directory || '—' }}
          </div>
        </section>

        <!-- Command Line -->
        <section class="space-y-1.5">
          <div class="flex items-center justify-between">
            <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Command Line</h3>
            <button
              type="button"
              (click)="copyCmdline(d.cmdline)"
              class="inline-flex items-center gap-1 px-2 py-1 text-xs font-sans rounded-sm bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-zinc-100 transition-colors focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400 cursor-pointer"
              [attr.aria-label]="isCopied() ? 'Command line copied' : 'Copy command line'"
            >
              @if (isCopied()) {
                <svg class="w-3.5 h-3.5 text-emerald-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                </svg>
                <span class="text-emerald-400 font-medium">Copied!</span>
              } @else {
                <svg class="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
                </svg>
                <span>Copy</span>
              }
            </button>
          </div>
          <div class="bg-zinc-950 border border-zinc-800 rounded p-2.5 text-xs font-mono text-zinc-300 break-all select-all max-h-40 overflow-y-auto">
            {{ d.cmdline || '—' }}
          </div>
        </section>
      } @else if (isLoading()) {
        <div class="flex items-center justify-center p-12 text-zinc-400 text-sm">
          <svg class="animate-spin -ml-1 mr-3 h-5 w-5 text-zinc-400" fill="none" viewBox="0 0 24 24">
            <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
            <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8v8H4z"></path>
          </svg>
          Loading process details...
        </div>
      } @else if (hasError()) {
        <np-empty-state
          title="Process Not Found"
          [description]="errorMessage() || 'The requested process (PID ' + pid() + ') may have exited or is not accessible.'"
        />
      }
    </np-drawer>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ProcessDetailDrawerComponent {
  private readonly apiClient = inject(ApiClientService);
  private readonly destroyRef = inject(DestroyRef);

  readonly pid = input<number | null>(null);
  readonly close = output<void>();

  readonly detail = signal<ProcessDetail | null>(null);
  readonly isLoading = signal<boolean>(false);
  readonly hasError = signal<boolean>(false);
  readonly errorMessage = signal<string | null>(null);
  readonly isCopied = signal<boolean>(false);

  private currentSub?: Subscription;
  private copyTimer?: ReturnType<typeof setTimeout>;

  readonly formatBytes = formatBytes;
  readonly formatEpochSeconds = formatEpochSeconds;
  readonly formatDurationSince = formatDurationSince;

  readonly isOpen = computed<boolean>(() => {
    const p = this.pid();
    return p !== null && p !== undefined && p > 0;
  });

  readonly title = computed<string>(() => {
    const d = this.detail();
    if (d) {
      return d.name;
    }
    const p = this.pid();
    if (p !== null && p !== undefined && p > 0) {
      return `PID ${p}`;
    }
    return 'Process Details';
  });

  readonly subtitle = computed<string | undefined>(() => {
    const d = this.detail();
    if (d) {
      return `PID ${d.pid} • User: ${d.user}`;
    }
    const p = this.pid();
    if (p !== null && p !== undefined && p > 0) {
      return `Process #${p}`;
    }
    return undefined;
  });

  constructor() {
    this.destroyRef.onDestroy(() => {
      if (this.copyTimer) {
        clearTimeout(this.copyTimer);
      }
      this.currentSub?.unsubscribe();
    });

    effect(() => {
      const p = this.pid();
      untracked(() => {
        if (p !== null && p !== undefined && p > 0) {
          this.fetchProcess(p);
        } else {
          this.currentSub?.unsubscribe();
          this.detail.set(null);
          this.isLoading.set(false);
          this.hasError.set(false);
          this.errorMessage.set(null);
        }
      });
    });
  }

  getStateStatus(state: string | undefined): string {
    switch (state?.toUpperCase()) {
      case 'R':
        return 'running';
      case 'Z':
        return 'critical';
      case 'S':
      case 'D':
      default:
        return 'inactive';
    }
  }

  copyCmdline(text: string): void {
    if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
      navigator.clipboard.writeText(text).then(() => {
        this.isCopied.set(true);
        if (this.copyTimer) {
          clearTimeout(this.copyTimer);
        }
        this.copyTimer = setTimeout(() => {
          this.isCopied.set(false);
        }, 2000);
      }).catch(() => {
        // clipboard failure fallback
      });
    }
  }

  onClose(): void {
    this.close.emit();
  }

  private fetchProcess(pid: number): void {
    this.currentSub?.unsubscribe();
    this.isLoading.set(true);
    this.hasError.set(false);
    this.errorMessage.set(null);
    this.detail.set(null);

    this.currentSub = this.apiClient.getProcess(pid).subscribe({
      next: (data) => {
        this.detail.set(data);
        this.isLoading.set(false);
        this.hasError.set(false);
      },
      error: (err) => {
        this.detail.set(null);
        this.isLoading.set(false);
        this.hasError.set(true);
        this.errorMessage.set(
          err?.message || `The requested process (PID ${pid}) may have exited or is not accessible.`
        );
      },
    });
  }
}
