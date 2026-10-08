import {
  ChangeDetectionStrategy,
  Component,
  DestroyRef,
  inject,
  OnInit,
  signal,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { Subscription } from 'rxjs';
import { ActivatedRoute, Router } from '@angular/router';
import { ApiClientService } from '../../core/api/api-client.service';
import { ContainerSummary } from '../../core/models/container.model';
import { DataTableComponent } from '../../shared/ui/data-table/data-table.component';
import { StatusBadgeComponent } from '../../shared/ui/status-badge/status-badge.component';
import { EmptyStateComponent } from '../../shared/ui/empty-state/empty-state.component';
import { formatEpochSeconds } from '../../shared/utils/formatters';
import { ContainerDetailDrawerComponent } from './components/container-detail-drawer/container-detail-drawer.component';

@Component({
  selector: 'np-containers',
  standalone: true,
  imports: [
    DataTableComponent,
    StatusBadgeComponent,
    EmptyStateComponent,
    ContainerDetailDrawerComponent,
  ],
  template: `
    <div class="space-y-4">
      <!-- Toolbar -->
      <header class="flex flex-col sm:flex-row sm:items-center justify-between gap-4 pb-2 border-b border-zinc-800">
        <div>
          <h1 class="text-xl font-bold tracking-tight text-zinc-100">Containers</h1>
          <p class="text-xs text-zinc-400 mt-0.5">
            Host Docker containers, status, and deep-linked inspection.
          </p>
        </div>

        <div class="flex items-center gap-3">
          <!-- Manual refresh -->
          <button
            type="button"
            (click)="refresh()"
            [disabled]="isLoading()"
            class="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded-sm border border-zinc-800 bg-zinc-900 hover:bg-zinc-800 text-zinc-200 hover:text-zinc-100 disabled:opacity-50 disabled:cursor-not-allowed transition-colors cursor-pointer focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
            aria-label="Refresh containers"
          >
            <svg
              class="w-3.5 h-3.5"
              [class.animate-spin]="isLoading()"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
            <span>Refresh</span>
          </button>
        </div>
      </header>

      <!-- Scenario B: Docker Unavailable (HTTP 503 / DOCKER_UNAVAILABLE) -->
      @if (isDockerUnavailable()) {
        <np-empty-state
          title="Docker Unavailable on Host"
          description="NodePulse could not communicate with /var/run/docker.sock. Docker may be stopped or not installed on this host."
        >
          <button
            type="button"
            (click)="refresh()"
            [disabled]="isLoading()"
            class="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded-sm border border-zinc-700 bg-zinc-800 hover:bg-zinc-700 text-zinc-200 hover:text-zinc-100 transition-colors cursor-pointer focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
            aria-label="Retry Docker connection"
          >
            <svg
              class="w-3.5 h-3.5"
              [class.animate-spin]="isLoading()"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
            <span>Retry Connection</span>
          </button>
        </np-empty-state>
      }

      <!-- Scenario C: General Error -->
      @else if (errorMessage()) {
        <np-empty-state
          title="Failed to Load Containers"
          [description]="errorMessage()!"
        >
          <button
            type="button"
            (click)="refresh()"
            [disabled]="isLoading()"
            class="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded-sm border border-zinc-700 bg-zinc-800 hover:bg-zinc-700 text-zinc-200 hover:text-zinc-100 transition-colors cursor-pointer focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
            aria-label="Retry loading containers"
          >
            <svg
              class="w-3.5 h-3.5"
              [class.animate-spin]="isLoading()"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              stroke-width="2"
            >
              <path stroke-linecap="round" stroke-linejoin="round" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
            <span>Retry</span>
          </button>
        </np-empty-state>
      }

      <!-- Scenario A: Empty List (HTTP 200, 0 containers) -->
      @else if (containers().length === 0 && !isLoading()) {
        <np-empty-state
          title="No Containers Found"
          description="No Docker containers are currently running or stopped on this host."
        />
      }

      <!-- Scenario A: Containers Table -->
      @else {
        <np-data-table
          [isEmpty]="containers().length === 0"
          emptyMessage="No containers found"
          [colSpan]="5"
          ariaLabel="Containers Table"
        >
          <tr table-header class="border-b border-zinc-800 text-zinc-400 text-left font-medium">
            <th scope="col" class="py-2.5 px-3">Container Name</th>
            <th scope="col" class="py-2.5 px-3">Image</th>
            <th scope="col" class="py-2.5 px-3 w-28">State</th>
            <th scope="col" class="py-2.5 px-3">Status</th>
            <th scope="col" class="py-2.5 px-3 w-48">Created Time</th>
          </tr>

          @for (c of containers(); track c.id) {
            <tr
              table-body
              tabindex="0"
              role="button"
              (click)="onSelectContainer(c)"
              (keydown.enter)="onSelectContainer(c)"
              (keydown.space)="onSelectContainer(c); $event.preventDefault()"
              class="hover:bg-zinc-800/50 cursor-pointer transition-colors focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
              [attr.aria-label]="'Select container ' + getContainerName(c)"
            >
              <td class="py-2.5 px-3 font-mono font-medium text-zinc-100">
                {{ getContainerName(c) }}
              </td>
              <td class="py-2.5 px-3 font-mono text-zinc-400 truncate max-w-xs md:max-w-sm">
                {{ c.image }}
              </td>
              <td class="py-2.5 px-3">
                <np-status-badge [status]="c.state" />
              </td>
              <td class="py-2.5 px-3 font-mono text-zinc-300">
                {{ c.status }}
              </td>
              <td class="py-2.5 px-3 font-mono text-zinc-400">
                {{ formatEpochSeconds(c.created) }}
              </td>
            </tr>
          }
        </np-data-table>
      }

      <!-- Inspection Drawer -->
      <np-container-detail-drawer
        [id]="selectedId()"
        (close)="onCloseDrawer()"
      />
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ContainersComponent implements OnInit {
  private readonly apiClient = inject(ApiClientService);
  private readonly route = inject(ActivatedRoute);
  private readonly router = inject(Router);
  private readonly destroyRef = inject(DestroyRef);

  readonly containers = signal<ContainerSummary[]>([]);
  readonly isLoading = signal<boolean>(false);
  readonly isDockerUnavailable = signal<boolean>(false);
  readonly errorMessage = signal<string | null>(null);
  readonly selectedId = signal<string | null>(null);

  private fetchSub?: Subscription;

  readonly formatEpochSeconds = formatEpochSeconds;

  constructor() {
    this.destroyRef.onDestroy(() => {
      this.fetchSub?.unsubscribe();
    });

    this.route.queryParamMap
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((params) => {
        const raw = params.get('id');
        if (raw !== null && raw.trim() !== '') {
          this.selectedId.set(raw.trim());
        } else {
          this.selectedId.set(null);
        }
      });
  }

  ngOnInit(): void {
    this.loadContainers();
  }

  refresh(): void {
    this.loadContainers();
  }

  loadContainers(): void {
    this.fetchSub?.unsubscribe();
    this.isLoading.set(true);
    this.isDockerUnavailable.set(false);
    this.errorMessage.set(null);

    this.fetchSub = this.apiClient.getContainers().subscribe({
      next: (data) => {
        this.containers.set(data || []);
        this.isLoading.set(false);
        this.isDockerUnavailable.set(false);
        this.errorMessage.set(null);
      },
      error: (err) => {
        this.containers.set([]);
        this.isLoading.set(false);
        const is503 = err?.status === 503;
        const code = err?.error?.code || err?.error?.error?.code;
        if (is503 || code === 'DOCKER_UNAVAILABLE') {
          this.isDockerUnavailable.set(true);
          this.errorMessage.set(null);
        } else {
          this.isDockerUnavailable.set(false);
          this.errorMessage.set(
            err?.error?.message ||
              err?.error?.error?.message ||
              err?.message ||
              'Failed to load containers'
          );
        }
      },
    });
  }

  onSelectContainer(container: ContainerSummary): void {
    this.router.navigate([], {
      queryParams: { id: container.id },
      queryParamsHandling: 'merge',
    });
  }

  onCloseDrawer(): void {
    this.router.navigate([], {
      queryParams: { id: null },
      queryParamsHandling: 'merge',
    });
  }

  getContainerName(container: ContainerSummary): string {
    if (!container.names || container.names.length === 0) {
      return '—';
    }
    const primary = container.names[0];
    return primary.startsWith('/') ? primary.slice(1) : primary;
  }
}
