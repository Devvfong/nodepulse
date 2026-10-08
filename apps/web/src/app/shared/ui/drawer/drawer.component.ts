import {
  ChangeDetectionStrategy,
  Component,
  ElementRef,
  HostListener,
  ViewChild,
  effect,
  input,
  output,
} from '@angular/core';

@Component({
  selector: 'np-drawer',
  standalone: true,
  changeDetection: ChangeDetectionStrategy.OnPush,
  template: `
    @if (isOpen()) {
      <div class="drawer-container fixed inset-0 z-40" role="presentation">
        <!-- Backdrop overlay -->
        <div
          class="drawer-backdrop fixed inset-0 bg-black/60 z-40 transition-opacity"
          aria-hidden="true"
          (click)="onBackdropClick()"
        ></div>

        <!-- Drawer panel -->
        <div
          #panel
          class="drawer-panel fixed top-0 right-0 bottom-0 h-full w-full max-w-full md:max-w-xl lg:max-w-[480px] bg-zinc-900 border-l border-zinc-800 z-50 flex flex-col shadow-2xl focus:outline-none"
          role="dialog"
          aria-modal="true"
          [attr.aria-label]="title()"
          tabindex="-1"
          (click)="$event.stopPropagation()"
        >
          <!-- Header -->
          <header
            class="drawer-header px-4 py-4 sm:px-6 border-b border-zinc-800 flex items-start justify-between gap-4 flex-shrink-0"
          >
            <div class="min-w-0 flex-1">
              <h2 class="drawer-title text-zinc-100 font-bold text-base truncate">
                {{ title() }}
              </h2>
              @if (subtitle()) {
                <p class="drawer-subtitle text-zinc-400 text-sm mt-0.5 truncate">
                  {{ subtitle() }}
                </p>
              }
            </div>

            <div class="drawer-header-actions flex items-center gap-2 flex-shrink-0">
              <ng-content select="[drawer-header]"></ng-content>
              <button
                type="button"
                class="drawer-close-btn p-1.5 rounded-sm text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800 focus:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400 transition-colors"
                aria-label="Close drawer"
                (click)="onCloseClick()"
              >
                <svg
                  class="w-5 h-5"
                  fill="none"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                  stroke-width="2"
                  aria-hidden="true"
                >
                  <path stroke-linecap="round" stroke-linejoin="round" d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            </div>
          </header>

          <!-- Body -->
          <div class="drawer-body flex-1 overflow-y-auto p-4 sm:p-6 text-zinc-100 space-y-4">
            <ng-content></ng-content>
          </div>
        </div>
      </div>
    }
  `,
})
export class DrawerComponent {
  readonly isOpen = input<boolean>(false);
  readonly title = input<string>('');
  readonly subtitle = input<string | undefined>(undefined);
  readonly close = output<void>();

  @ViewChild('panel') panelElement?: ElementRef<HTMLElement>;

  private previousActiveElement: HTMLElement | null = null;

  constructor() {
    effect(() => {
      if (this.isOpen()) {
        if (typeof document !== 'undefined') {
          this.previousActiveElement = document.activeElement as HTMLElement | null;
        }
        queueMicrotask(() => {
          this.panelElement?.nativeElement?.focus();
        });
      } else {
        if (this.previousActiveElement) {
          const toRestore = this.previousActiveElement;
          this.previousActiveElement = null;
          queueMicrotask(() => {
            toRestore.focus();
          });
        }
      }
    });
  }

  @HostListener('window:keydown', ['$event'])
  handleKeyDown(event: Event): void {
    if (!this.isOpen()) {
      return;
    }
    const kbEvent = event as KeyboardEvent;
    if (kbEvent.key === 'Escape') {
      kbEvent.preventDefault();
      this.close.emit();
    } else if (kbEvent.key === 'Tab') {
      this.trapTab(kbEvent);
    }
  }

  private trapTab(event: KeyboardEvent): void {
    if (!this.panelElement?.nativeElement) {
      return;
    }

    const panel = this.panelElement.nativeElement;
    const focusableSelectors =
      'button:not([disabled]), [href], input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';
    const focusables = Array.from(
      panel.querySelectorAll<HTMLElement>(focusableSelectors),
    );

    if (focusables.length === 0) {
      event.preventDefault();
      return;
    }

    const first = focusables[0];
    const last = focusables[focusables.length - 1];

    if (event.shiftKey) {
      if (document.activeElement === first || document.activeElement === panel) {
        event.preventDefault();
        last.focus();
      }
    } else {
      if (document.activeElement === last) {
        event.preventDefault();
        first.focus();
      }
    }
  }

  onBackdropClick(): void {
    this.close.emit();
  }

  onCloseClick(): void {
    this.close.emit();
  }
}
