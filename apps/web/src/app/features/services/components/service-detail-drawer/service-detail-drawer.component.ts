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
import { Router } from '@angular/router';
import { Subscription } from 'rxjs';
import { ApiClientService } from '../../../../core/api/api-client.service';
import { ServiceDetail } from '../../../../core/models/service.model';
import { DrawerComponent } from '../../../../shared/ui/drawer/drawer.component';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { EmptyStateComponent } from '../../../../shared/ui/empty-state/empty-state.component';
import { formatBytes, formatDurationSince, formatEpochSeconds } from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-service-detail-drawer',
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
        <div drawer-header class="flex items-center gap-2 flex-wrap">
          <np-status-badge [status]="d.active_state" />
          <span
            class="font-mono text-xs px-2 py-0.5 rounded-sm border"
            [class]="d.load_state === 'loaded' ? 'bg-zinc-800 text-zinc-300 border-zinc-700' : 'bg-rose-950/40 text-rose-400 border-rose-800/50'"
          >
            {{ d.load_state }}
          </span>
          <span class="font-mono text-xs px-2 py-0.5 rounded-sm bg-zinc-800 text-zinc-300 border border-zinc-700">
            {{ d.sub_state }}
          </span>
        </div>
      }

      @if (detail(); as d) {
        <!-- Description -->
        @if (d.description) {
          <section class="space-y-1.5">
            <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Description</h3>
            <p class="text-sm text-zinc-200">{{ d.description }}</p>
          </section>
        }

        <!-- State & Configuration -->
        <section class="space-y-2">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">State & Configuration</h3>
          <dl class="grid grid-cols-2 gap-2 text-xs">
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Active State</dt>
              <dd class="mt-1">
                <np-status-badge [status]="d.active_state" />
              </dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Sub-State</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ d.sub_state || '—' }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Load State</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ d.load_state || '—' }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Unit File State</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ d.unit_file_state || '—' }}</dd>
            </div>
          </dl>
        </section>

        <!-- Runtime Details -->
        <section class="space-y-2">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Runtime Details</h3>
          <dl class="grid grid-cols-2 gap-2 text-xs">
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Main PID</dt>
              <dd class="mt-1">
                @if (d.main_pid >= 1) {
                  <button
                    type="button"
                    (click)="navigateToProcess(d.main_pid)"
                    class="text-sky-400 hover:underline font-mono cursor-pointer inline-flex items-center gap-1 focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
                    [attr.aria-label]="'Inspect process with PID ' + d.main_pid"
                  >
                    {{ d.main_pid }}
                  </button>
                } @else {
                  <span class="font-mono text-zinc-400"> — </span>
                }
              </dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Restart Count</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ d.restart_count }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Current Memory</dt>
              <dd class="font-mono text-zinc-200 mt-1">{{ formatBytes(d.memory_current_bytes) }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Active Since</dt>
              <dd class="font-mono text-zinc-200 mt-1">
                @if (d.active_enter_timestamp_utc > 0) {
                  {{ formatEpochSeconds(d.active_enter_timestamp_utc) }}
                  <span class="text-zinc-400 block sm:inline">({{ formatDurationSince(d.active_enter_timestamp_utc) }} ago)</span>
                } @else {
                  <span class="text-zinc-400"> — </span>
                }
              </dd>
            </div>
          </dl>
        </section>
      } @else if (isLoading()) {
        <div class="flex items-center justify-center p-12 text-zinc-400 text-sm">
          <svg class="animate-spin -ml-1 mr-3 h-5 w-5 text-zinc-400" fill="none" viewBox="0 0 24 24">
            <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
            <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8v8H4z"></path>
          </svg>
          Loading service details...
        </div>
      } @else if (hasError()) {
        <np-empty-state
          title="Service Not Found"
          [description]="errorMessage() || 'The requested service (' + name() + ') was not found or is inaccessible.'"
        />
      }
    </np-drawer>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ServiceDetailDrawerComponent {
  private readonly apiClient = inject(ApiClientService);
  private readonly router = inject(Router);
  private readonly destroyRef = inject(DestroyRef);

  readonly name = input<string | null>(null);
  readonly close = output<void>();

  readonly detail = signal<ServiceDetail | null>(null);
  readonly isLoading = signal<boolean>(false);
  readonly hasError = signal<boolean>(false);
  readonly errorMessage = signal<string | null>(null);

  private currentSub?: Subscription;

  readonly formatBytes = formatBytes;
  readonly formatEpochSeconds = formatEpochSeconds;
  readonly formatDurationSince = formatDurationSince;

  readonly isOpen = computed<boolean>(() => {
    const n = this.name();
    return n !== null && n !== undefined && n.trim().length > 0;
  });

  readonly title = computed<string>(() => {
    const d = this.detail();
    if (d) {
      return d.name;
    }
    const n = this.name();
    if (n && n.trim().length > 0) {
      return n.trim();
    }
    return 'Service Details';
  });

  readonly subtitle = computed<string | undefined>(() => {
    const d = this.detail();
    if (d) {
      return d.description || d.name;
    }
    return undefined;
  });

  constructor() {
    this.destroyRef.onDestroy(() => {
      this.currentSub?.unsubscribe();
    });

    effect(() => {
      const n = this.name();
      untracked(() => {
        if (n && n.trim().length > 0) {
          this.fetchService(n.trim());
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

  onClose(): void {
    this.close.emit();
  }

  navigateToProcess(pid: number): void {
    if (pid >= 1) {
      this.router.navigate(['/processes'], {
        queryParams: { pid },
      });
    }
  }

  private fetchService(name: string): void {
    this.currentSub?.unsubscribe();
    this.isLoading.set(true);
    this.hasError.set(false);
    this.errorMessage.set(null);
    this.detail.set(null);

    this.currentSub = this.apiClient.getServiceDetail(name).subscribe({
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
          err?.message || `The requested service (${name}) was not found or is inaccessible.`
        );
      },
    });
  }
}
