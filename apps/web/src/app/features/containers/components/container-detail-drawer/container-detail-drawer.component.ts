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
import { ContainerDetail } from '../../../../core/models/container.model';
import { DrawerComponent } from '../../../../shared/ui/drawer/drawer.component';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { EmptyStateComponent } from '../../../../shared/ui/empty-state/empty-state.component';
import { formatDurationSince, formatEpochSeconds } from '../../../../shared/utils/formatters';

@Component({
  selector: 'np-container-detail-drawer',
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
          <np-status-badge [status]="d.state" />
          <span
            class="font-mono text-xs px-2 py-0.5 rounded-sm border"
            [class]="d.running ? 'bg-emerald-950/40 text-emerald-400 border-emerald-800/50' : 'bg-zinc-800 text-zinc-400 border-zinc-700'"
          >
            {{ d.running ? 'RUNNING' : 'STOPPED' }}
          </span>
        </div>
      }

      @if (detail(); as d) {
        <!-- Container ID -->
        <section class="space-y-1.5">
          <div class="flex items-center justify-between">
            <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Container ID</h3>
            <button
              type="button"
              (click)="copyId(d.id)"
              class="inline-flex items-center gap-1 px-2 py-1 text-xs font-sans rounded-sm bg-zinc-800 hover:bg-zinc-700 text-zinc-300 hover:text-zinc-100 transition-colors focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400 cursor-pointer"
              [attr.aria-label]="isCopied() ? 'Container ID copied' : 'Copy container ID'"
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
          <div class="bg-zinc-950 border border-zinc-800 rounded p-2.5 text-xs font-mono text-zinc-300 break-all select-all">
            {{ d.id }}
          </div>
        </section>

        <!-- Configuration & Runtime -->
        <section class="space-y-2">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Configuration & Runtime</h3>
          <dl class="grid grid-cols-2 gap-2 text-xs">
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5 col-span-2">
              <dt class="text-zinc-400">Image</dt>
              <dd class="font-mono text-xs text-zinc-300 break-all mt-1">{{ d.image || '—' }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Status</dt>
              <dd class="text-zinc-300 font-mono text-sm mt-1">{{ d.status || '—' }}</dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5">
              <dt class="text-zinc-400">Exit Code</dt>
              <dd
                class="mt-1 font-mono text-sm"
                [class]="d.exit_code === 0 ? 'text-zinc-100 font-mono' : 'text-rose-400 font-mono font-semibold'"
              >
                {{ d.exit_code }}
              </dd>
            </div>
            <div class="bg-zinc-950/60 border border-zinc-800 rounded p-2.5 col-span-2">
              <dt class="text-zinc-400">Created</dt>
              <dd class="font-mono text-zinc-200 mt-1">
                @if (d.created > 0) {
                  {{ formatEpochSeconds(d.created) }}
                  <span class="text-zinc-400 block sm:inline">({{ formatDurationSince(d.created) }} ago)</span>
                } @else {
                  <span class="text-zinc-400"> — </span>
                }
              </dd>
            </div>
          </dl>
        </section>

        <!-- Port Mappings -->
        <section class="space-y-1.5">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Port Mappings</h3>
          @if (d.port_mappings && d.port_mappings.length > 0) {
            <div class="flex flex-wrap gap-1.5">
              @for (port of d.port_mappings; track port) {
                <span class="font-mono text-xs bg-zinc-800/80 px-2 py-1 rounded border border-zinc-700/60 text-zinc-200">
                  {{ port }}
                </span>
              }
            </div>
          } @else {
            <p class="text-zinc-500 italic text-xs">No port mappings</p>
          }
        </section>

        <!-- Mount Points / Volumes -->
        <section class="space-y-1.5">
          <h3 class="text-xs font-semibold text-zinc-400 uppercase tracking-wider">Mount Points & Volumes</h3>
          @if (d.mount_sources && d.mount_sources.length > 0) {
            <div class="flex flex-col gap-1.5">
              @for (mount of d.mount_sources; track mount) {
                <span class="font-mono text-xs bg-zinc-800/80 px-2 py-1 rounded border border-zinc-700/60 text-zinc-200 break-all">
                  {{ mount }}
                </span>
              }
            </div>
          } @else {
            <p class="text-zinc-500 italic text-xs">No volume mounts</p>
          }
        </section>
      } @else if (isLoading()) {
        <div class="flex items-center justify-center p-12 text-zinc-400 text-sm">
          <svg class="animate-spin -ml-1 mr-3 h-5 w-5 text-zinc-400" fill="none" viewBox="0 0 24 24">
            <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
            <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8v8H4z"></path>
          </svg>
          Loading container details...
        </div>
      } @else if (hasError()) {
        <np-empty-state
          title="Container Not Found"
          [description]="errorMessage() || 'The requested container (' + id() + ') was not found or is inaccessible.'"
        />
      }
    </np-drawer>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ContainerDetailDrawerComponent {
  private readonly apiClient = inject(ApiClientService);
  private readonly destroyRef = inject(DestroyRef);

  readonly id = input<string | null>(null);
  readonly close = output<void>();

  readonly detail = signal<ContainerDetail | null>(null);
  readonly isLoading = signal<boolean>(false);
  readonly hasError = signal<boolean>(false);
  readonly errorMessage = signal<string | null>(null);
  readonly isCopied = signal<boolean>(false);

  private currentSub?: Subscription;
  private copyTimer?: ReturnType<typeof setTimeout>;

  readonly formatEpochSeconds = formatEpochSeconds;
  readonly formatDurationSince = formatDurationSince;

  readonly isOpen = computed<boolean>(() => {
    const idVal = this.id();
    return idVal !== null && idVal !== undefined && idVal.trim().length > 0;
  });

  readonly title = computed<string>(() => {
    const d = this.detail();
    if (d) {
      const rawName = d.name || '';
      const cleanName = rawName.startsWith('/') ? rawName.slice(1) : rawName;
      return cleanName || d.id.slice(0, 12);
    }
    const idVal = this.id();
    if (idVal && idVal.trim().length > 0) {
      return idVal.trim().slice(0, 12);
    }
    return 'Container Details';
  });

  readonly subtitle = computed<string | undefined>(() => {
    const d = this.detail();
    if (d) {
      return d.image;
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
      const idVal = this.id();
      untracked(() => {
        if (idVal && idVal.trim().length > 0) {
          this.fetchContainer(idVal.trim());
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

  copyId(idText: string): void {
    if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
      navigator.clipboard.writeText(idText).then(() => {
        this.isCopied.set(true);
        if (this.copyTimer) {
          clearTimeout(this.copyTimer);
        }
        this.copyTimer = setTimeout(() => {
          this.isCopied.set(false);
        }, 2000);
      }).catch(() => {});
    }
  }

  private fetchContainer(containerId: string): void {
    this.currentSub?.unsubscribe();
    this.isLoading.set(true);
    this.hasError.set(false);
    this.errorMessage.set(null);
    this.detail.set(null);

    this.currentSub = this.apiClient.getContainerDetail(containerId).subscribe({
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
          err?.message || `The requested container (${containerId}) was not found or is inaccessible.`
        );
      },
    });
  }
}
