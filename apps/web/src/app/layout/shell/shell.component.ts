import {
  ChangeDetectionStrategy,
  Component,
  HostListener,
  signal,
} from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { SidebarComponent } from '../sidebar/sidebar.component';
import { HeaderComponent } from '../header/header.component';

@Component({
  selector: 'np-shell',
  standalone: true,
  imports: [RouterOutlet, SidebarComponent, HeaderComponent],
  template: `
    <div class="flex h-screen w-screen overflow-hidden bg-zinc-950 text-zinc-100">
      <!-- Desktop Persistent Sidebar (w-60 hidden lg:flex) -->
      <np-sidebar class="hidden lg:flex w-60 h-full shrink-0" />

      <!-- Mobile Navigation Drawer / Overlay Sheet -->
      @if (mobileNavOpen()) {
        <div
          class="fixed inset-0 z-50 lg:hidden flex"
          role="dialog"
          aria-modal="true"
          aria-label="Mobile Navigation"
        >
          <!-- Backdrop -->
          <div
            class="mobile-nav-backdrop fixed inset-0 bg-black/70 transition-opacity"
            aria-hidden="true"
            (click)="closeMobileNav()"
          ></div>

          <!-- Slide-over Drawer Panel -->
          <div class="relative flex flex-col w-60 max-w-[80vw] h-full bg-zinc-900 shadow-xl z-10">
            <np-sidebar (linkClick)="closeMobileNav()" class="h-full flex flex-col" />
          </div>
        </div>
      }

      <!-- Main Layout Column -->
      <div class="flex flex-col flex-1 min-w-0 h-full overflow-hidden">
        <np-header
          [isMenuOpen]="mobileNavOpen()"
          (menuToggle)="toggleMobileNav()"
        />
        <main class="flex-1 overflow-y-auto p-4 sm:p-6">
          <router-outlet />
        </main>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class ShellComponent {
  readonly mobileNavOpen = signal<boolean>(false);

  @HostListener('window:keydown', ['$event'])
  onKeydown(event: Event): void {
    const keyEvent = event as KeyboardEvent;
    if (keyEvent.key === 'Escape' && this.mobileNavOpen()) {
      this.closeMobileNav();
    }
  }

  toggleMobileNav(): void {
    this.mobileNavOpen.update((open) => !open);
  }

  closeMobileNav(): void {
    this.mobileNavOpen.set(false);
  }

  openMobileNav(): void {
    this.mobileNavOpen.set(true);
  }
}
