import {
  ChangeDetectionStrategy,
  Component,
  DestroyRef,
  inject,
  OnInit,
  signal,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, Router } from '@angular/router';
import { ApiClientService } from '../../core/api/api-client.service';
import { ProcessInfo } from '../../core/models/process.model';
import { DataTableComponent } from '../../shared/ui/data-table/data-table.component';
import { StatusBadgeComponent } from '../../shared/ui/status-badge/status-badge.component';
import { formatBytes } from '../../shared/utils/formatters';
import { ProcessDetailDrawerComponent } from './components/process-detail-drawer/process-detail-drawer.component';

@Component({
  selector: 'np-processes',
  standalone: true,
  imports: [
    DataTableComponent,
    StatusBadgeComponent,
    ProcessDetailDrawerComponent,
  ],
  template: `
    <div class="space-y-4">
      <!-- Toolbar -->
      <header class="flex flex-col sm:flex-row sm:items-center justify-between gap-4 pb-2 border-b border-zinc-800">
        <div>
          <h1 class="text-xl font-bold tracking-tight text-zinc-100">Processes</h1>
          <p class="text-xs text-zinc-400 mt-0.5">
            Host process telemetry, resource utilization, and deep-linked inspection.
          </p>
        </div>

        <div class="flex flex-wrap items-center gap-3">
          <!-- Sort controls -->
          <div class="flex items-center gap-1.5 bg-zinc-900 border border-zinc-800 rounded p-1">
            <span class="text-xs text-zinc-400 px-2 font-medium">Sort:</span>
            <button
              type="button"
              (click)="setSort('cpu')"
              [class]="sort() === 'cpu' ? 'bg-zinc-800 text-zinc-100 font-semibold border-zinc-700' : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/40 border-transparent'"
              class="px-2.5 py-1 text-xs rounded-sm border transition-colors cursor-pointer"
              aria-label="Sort by CPU %"
            >
              CPU %
            </button>
            <button
              type="button"
              (click)="setSort('memory')"
              [class]="sort() === 'memory' ? 'bg-zinc-800 text-zinc-100 font-semibold border-zinc-700' : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/40 border-transparent'"
              class="px-2.5 py-1 text-xs rounded-sm border transition-colors cursor-pointer"
              aria-label="Sort by Memory"
            >
              Memory
            </button>
            <button
              type="button"
              (click)="setSort('pid')"
              [class]="sort() === 'pid' ? 'bg-zinc-800 text-zinc-100 font-semibold border-zinc-700' : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/40 border-transparent'"
              class="px-2.5 py-1 text-xs rounded-sm border transition-colors cursor-pointer"
              aria-label="Sort by PID"
            >
              PID
            </button>
          </div>

          <!-- Limit controls -->
          <div class="flex items-center gap-1.5 bg-zinc-900 border border-zinc-800 rounded p-1">
            <span class="text-xs text-zinc-400 px-2 font-medium">Limit:</span>
            @for (l of limitOptions; track l) {
              <button
                type="button"
                (click)="setLimit(l)"
                [class]="limit() === l ? 'bg-zinc-800 text-zinc-100 font-semibold border-zinc-700' : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/40 border-transparent'"
                class="px-2 py-1 text-xs rounded-sm border font-mono transition-colors cursor-pointer"
                [attr.aria-label]="'Limit to ' + l + ' processes'"
              >
                {{ l }}
              </button>
            }
          </div>

          <!-- Manual refresh -->
          <button
            type="button"
            (click)="refresh()"
            [disabled]="isLoading()"
            class="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded-sm border border-zinc-800 bg-zinc-900 hover:bg-zinc-800 text-zinc-200 hover:text-zinc-100 disabled:opacity-50 disabled:cursor-not-allowed transition-colors cursor-pointer focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
            aria-label="Refresh processes"
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

      <!-- Process Table -->
      <np-data-table
        [isEmpty]="processes().length === 0"
        emptyMessage="No processes found"
        [colSpan]="7"
        ariaLabel="Processes Table"
      >
        <tr table-header class="border-b border-zinc-800 text-zinc-400 text-left font-medium">
          <th scope="col" class="py-2.5 px-3 w-20">PID</th>
          <th scope="col" class="py-2.5 px-3">Name</th>
          <th scope="col" class="py-2.5 px-3 hidden sm:table-cell">User</th>
          <th scope="col" class="py-2.5 px-3 w-24">State</th>
          <th scope="col" class="py-2.5 px-3 text-right w-24">CPU %</th>
          <th scope="col" class="py-2.5 px-3 text-right w-32">Memory (RSS)</th>
          <th scope="col" class="py-2.5 px-3 hidden md:table-cell">Command Line</th>
        </tr>

        @for (proc of processes(); track proc.pid) {
          <tr
            table-body
            tabindex="0"
            role="button"
            (click)="onSelectProcess(proc)"
            (keydown.enter)="onSelectProcess(proc)"
            (keydown.space)="onSelectProcess(proc); $event.preventDefault()"
            class="hover:bg-zinc-800/50 cursor-pointer transition-colors focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
            [attr.aria-label]="'Select process ' + proc.name + ' with PID ' + proc.pid"
          >
            <td class="py-2.5 px-3 font-mono text-zinc-300">{{ proc.pid }}</td>
            <td class="py-2.5 px-3 font-medium text-zinc-100">{{ proc.name }}</td>
            <td class="py-2.5 px-3 text-zinc-400 hidden sm:table-cell">{{ proc.user }}</td>
            <td class="py-2.5 px-3">
              <np-status-badge [status]="getStateStatus(proc.state)" [label]="proc.state" />
            </td>
            <td class="py-2.5 px-3 text-right font-mono font-semibold text-zinc-100">
              {{ proc.cpu_percent.toFixed(1) }}%
            </td>
            <td class="py-2.5 px-3 text-right font-mono text-zinc-300">
              {{ formatBytes(proc.memory_rss_bytes) }}
            </td>
            <td class="py-2.5 px-3 font-mono text-zinc-400 truncate max-w-xs md:max-w-md hidden md:table-cell">
              {{ proc.cmdline }}
            </td>
          </tr>
        }
      </np-data-table>

      <!-- Inspection Drawer -->
      <np-process-detail-drawer
        [pid]="selectedPid()"
        (close)="onCloseDrawer()"
      />
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ProcessesComponent implements OnInit {
  private readonly apiClient = inject(ApiClientService);
  private readonly route = inject(ActivatedRoute);
  private readonly router = inject(Router);
  private readonly destroyRef = inject(DestroyRef);

  readonly sort = signal<'cpu' | 'memory' | 'pid'>('cpu');
  readonly limit = signal<number>(50);
  readonly processes = signal<ProcessInfo[]>([]);
  readonly isLoading = signal<boolean>(false);
  readonly selectedPid = signal<number | null>(null);

  readonly limitOptions = [25, 50, 100, 200];
  readonly formatBytes = formatBytes;

  constructor() {
    this.route.queryParamMap
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((params) => {
        const raw = params.get('pid');
        if (raw !== null && raw !== '') {
          const num = Number(raw);
          this.selectedPid.set(!isNaN(num) && Number.isInteger(num) && num > 0 ? num : null);
        } else {
          this.selectedPid.set(null);
        }
      });
  }

  ngOnInit(): void {
    this.loadProcesses();
  }

  setSort(s: 'cpu' | 'memory' | 'pid'): void {
    if (this.sort() !== s) {
      this.sort.set(s);
      this.loadProcesses();
    }
  }

  setLimit(l: number): void {
    if (this.limit() !== l) {
      this.limit.set(l);
      this.loadProcesses();
    }
  }

  refresh(): void {
    this.loadProcesses();
  }

  loadProcesses(): void {
    this.isLoading.set(true);
    this.apiClient.getProcesses(this.sort(), this.limit()).subscribe({
      next: (data) => {
        this.processes.set(data || []);
        this.isLoading.set(false);
      },
      error: () => {
        this.processes.set([]);
        this.isLoading.set(false);
      },
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

  onSelectProcess(proc: ProcessInfo): void {
    this.router.navigate([], {
      queryParams: { pid: proc.pid },
      queryParamsHandling: 'merge',
    });
  }

  onCloseDrawer(): void {
    this.router.navigate([], {
      queryParams: { pid: null },
      queryParamsHandling: 'merge',
    });
  }
}
