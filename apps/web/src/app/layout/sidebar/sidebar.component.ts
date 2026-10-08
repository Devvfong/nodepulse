import {
  ChangeDetectionStrategy,
  Component,
  computed,
  inject,
  output,
} from '@angular/core';
import { RouterLink, RouterLinkActive } from '@angular/router';
import { HostStateService } from '../../core/services/host-state.service';

@Component({
  selector: 'np-sidebar',
  standalone: true,
  imports: [RouterLink, RouterLinkActive],
  template: `
    <aside class="w-60 h-full bg-zinc-900 border-r border-zinc-800 flex flex-col shrink-0">
      <!-- Branding & Host Identity -->
      <div class="h-14 px-4 border-b border-zinc-800 flex flex-col justify-center">
        <div class="flex items-center gap-2">
          <span class="w-2 h-2 rounded-full bg-emerald-400 shrink-0" aria-hidden="true"></span>
          <span class="font-bold tracking-tight text-zinc-100 text-base">NodePulse</span>
        </div>
        <span
          class="font-mono text-xs text-zinc-400 truncate mt-0.5"
          [title]="hostname()"
        >
          {{ hostname() }}
        </span>
      </div>

      <!-- Navigation Links -->
      <nav class="flex-1 px-3 py-4 space-y-1 overflow-y-auto">
        <a
          routerLink="/overview"
          routerLinkActive="bg-zinc-800 !text-zinc-100 font-semibold"
          [routerLinkActiveOptions]="{ exact: false }"
          [ariaCurrentWhenActive]="'page'"
          (click)="onLinkClick()"
          class="flex items-center gap-3 px-3 py-2 rounded text-sm text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800/50 transition-colors"
        >
          <svg
            class="w-4 h-4 shrink-0 text-zinc-400"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            stroke-width="2"
            aria-hidden="true"
          >
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              d="M3 13h8V3H3v10zm0 8h8v-6H3v6zm10 0h8V11h-8v10zm0-18v6h8V3h-8z"
            />
          </svg>
          <span>Overview</span>
        </a>

        <a
          routerLink="/processes"
          routerLinkActive="bg-zinc-800 !text-zinc-100 font-semibold"
          [routerLinkActiveOptions]="{ exact: false }"
          [ariaCurrentWhenActive]="'page'"
          (click)="onLinkClick()"
          class="flex items-center gap-3 px-3 py-2 rounded text-sm text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800/50 transition-colors"
        >
          <svg
            class="w-4 h-4 shrink-0 text-zinc-400"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            stroke-width="2"
            aria-hidden="true"
          >
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              d="M9 3v2m6-2v2M9 19v2m6-2v2M3 9h2m-2 6h2m14-6h2m-2 6h2M7 19h10a2 2 0 002-2V7a2 2 0 00-2-2H7a2 2 0 00-2 2v10a2 2 0 002 2zM9 9h6v6H9V9z"
            />
          </svg>
          <span>Processes</span>
        </a>

        <a
          routerLink="/services"
          routerLinkActive="bg-zinc-800 !text-zinc-100 font-semibold"
          [routerLinkActiveOptions]="{ exact: false }"
          [ariaCurrentWhenActive]="'page'"
          (click)="onLinkClick()"
          class="flex items-center gap-3 px-3 py-2 rounded text-sm text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800/50 transition-colors"
        >
          <svg
            class="w-4 h-4 shrink-0 text-zinc-400"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            stroke-width="2"
            aria-hidden="true"
          >
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10"
            />
          </svg>
          <span>Services</span>
        </a>

        <a
          routerLink="/containers"
          routerLinkActive="bg-zinc-800 !text-zinc-100 font-semibold"
          [routerLinkActiveOptions]="{ exact: false }"
          [ariaCurrentWhenActive]="'page'"
          (click)="onLinkClick()"
          class="flex items-center gap-3 px-3 py-2 rounded text-sm text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800/50 transition-colors"
        >
          <svg
            class="w-4 h-4 shrink-0 text-zinc-400"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            stroke-width="2"
            aria-hidden="true"
          >
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              d="M20 7l-8-4-8 4m16 0l-8 4m8-4v10l-8 4m0-10L4 7m8 4v10M4 7v10l8 4"
            />
          </svg>
          <span>Containers</span>
        </a>
      </nav>
    </aside>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class SidebarComponent {
  private readonly hostState = inject(HostStateService);

  readonly linkClick = output<void>();

  readonly hostname = computed(
    () => this.hostState.systemInfo()?.hostname ?? 'localhost',
  );

  onLinkClick(): void {
    this.linkClick.emit();
  }
}
