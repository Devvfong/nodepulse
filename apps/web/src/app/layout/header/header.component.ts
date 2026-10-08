import {
  ChangeDetectionStrategy,
  Component,
  inject,
  input,
  output,
} from '@angular/core';
import { ConnectionStateService } from '../../core/services/connection-state.service';
import { SseService } from '../../core/api/sse.service';
import { StatusBadgeComponent } from '../../shared/ui/status-badge/status-badge.component';

@Component({
  selector: 'np-header',
  standalone: true,
  imports: [StatusBadgeComponent],
  template: `
    <header class="h-14 border-b border-zinc-800 bg-zinc-900/60 px-4 flex items-center justify-between shrink-0">
      <!-- Left: Mobile Hamburger Toggle -->
      <div class="flex items-center gap-3">
        <button
          type="button"
          aria-label="Toggle navigation menu"
          [attr.aria-expanded]="isMenuOpen()"
          (click)="onMenuToggle()"
          class="lg:hidden p-1.5 rounded text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800 focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400 cursor-pointer"
        >
          <svg
            class="w-5 h-5"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            stroke-width="2"
            aria-hidden="true"
          >
            <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h16M4 18h16" />
          </svg>
        </button>
      </div>

      <!-- Right: Decoupled Status Badges & Stream Controls -->
      <div class="flex items-center gap-2 sm:gap-3 flex-wrap">
        <!-- Independent Agent Status Badge -->
        <np-status-badge
          [status]="connectionState.agentStatus()"
          [label]="'AGENT: ' + connectionState.agentStatus().toUpperCase()"
        />

        <!-- Independent Stream Status Badge -->
        <np-status-badge
          [status]="connectionState.streamStatus()"
          [label]="'STREAM: ' + connectionState.streamStatus().toUpperCase()"
        />

        <!-- Stream Pause / Resume Action -->
        <button
          type="button"
          (click)="toggleStream()"
          class="stream-toggle-btn font-mono text-xs px-2.5 py-1 rounded-sm border border-zinc-700 bg-zinc-800 text-zinc-200 hover:bg-zinc-700 transition-colors focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400 cursor-pointer"
          [attr.aria-label]="connectionState.isStreamPaused() ? 'Resume telemetry stream' : 'Pause telemetry stream'"
        >
          {{ connectionState.isStreamPaused() ? 'Resume' : 'Pause' }}
        </button>
      </div>
    </header>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class HeaderComponent {
  readonly connectionState = inject(ConnectionStateService);
  private readonly sseService = inject(SseService);

  readonly isMenuOpen = input<boolean>(false);
  readonly menuToggle = output<void>();

  onMenuToggle(): void {
    this.menuToggle.emit();
  }

  toggleStream(): void {
    if (this.connectionState.isStreamPaused()) {
      this.sseService.resume();
    } else {
      this.sseService.pause();
    }
  }
}
