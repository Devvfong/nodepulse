import {
  ChangeDetectionStrategy,
  Component,
  DestroyRef,
  inject,
  OnInit,
} from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { HostStateService } from '../../core/services/host-state.service';
import { HostVitalsComponent } from './components/host-vitals/host-vitals.component';
import { CpuCardComponent } from './components/cpu-card/cpu-card.component';
import { MemoryCardComponent } from './components/memory-card/memory-card.component';
import { NetworkCardComponent } from './components/network-card/network-card.component';
import { DisksCardComponent } from './components/disks-card/disks-card.component';
import { TopProcessesComponent } from './components/top-processes/top-processes.component';

@Component({
  selector: 'np-overview',
  standalone: true,
  imports: [
    HostVitalsComponent,
    CpuCardComponent,
    MemoryCardComponent,
    NetworkCardComponent,
    DisksCardComponent,
    TopProcessesComponent,
  ],
  template: `
    <div class="space-y-6">
      <!-- 1. System Identity & Host Vitals -->
      <np-host-vitals />

      <!-- 2. Subsystem Telemetry 2x2 Grid -->
      <div class="grid grid-cols-1 lg:grid-cols-2 gap-4 sm:gap-6">
        <np-cpu-card />
        <np-memory-card />
        <np-network-card />
        <np-disks-card />
      </div>

      <!-- 3. Top Processes by CPU -->
      <np-top-processes />
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class OverviewComponent implements OnInit {
  private readonly hostState = inject(HostStateService);
  private readonly destroyRef = inject(DestroyRef);

  ngOnInit(): void {
    // Snapshot sync on route entry
    this.hostState
      .fetchInitialVitals()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        error: () => {
          // Errors are captured within HostStateService lastError signal and ConnectionStateService
        },
      });
  }
}
