import { ChangeDetectionStrategy, Component, input } from '@angular/core';

@Component({
  selector: 'np-empty-state',
  standalone: true,
  template: `
    <div
      class="border border-dashed border-zinc-800 rounded p-8 text-center bg-zinc-900/40 flex flex-col items-center justify-center"
    >
      @if (icon()) {
        <div class="empty-state-icon mb-3 text-zinc-500 text-2xl" aria-hidden="true">
          {{ icon() }}
        </div>
      }

      <h3 class="empty-state-title text-zinc-200 text-base font-medium">
        {{ title() }}
      </h3>

      @if (description()) {
        <p class="empty-state-description text-zinc-400 text-sm mt-1 max-w-md">
          {{ description() }}
        </p>
      }

      <div class="empty-state-actions empty:hidden mt-4">
        <ng-content></ng-content>
      </div>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class EmptyStateComponent {
  readonly title = input<string>('');
  readonly description = input<string | undefined>(undefined);
  readonly icon = input<string | undefined>(undefined);
}
