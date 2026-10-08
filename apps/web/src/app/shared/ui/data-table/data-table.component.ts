import { ChangeDetectionStrategy, Component, computed, input } from '@angular/core';

@Component({
  selector: 'np-data-table',
  standalone: true,
  template: `
    <div class="overflow-x-auto border border-zinc-800 rounded bg-zinc-900">
      <table
        class="w-full text-left border-collapse text-sm text-zinc-100"
        [attr.aria-label]="ariaLabel()"
      >
        <thead class="bg-zinc-900 border-b border-zinc-800 text-xs font-medium text-zinc-400 uppercase tracking-wider">
          <ng-content select="[table-header]"></ng-content>
        </thead>
        <tbody class="divide-y divide-zinc-800/60 font-mono text-xs">
          @if (isEmpty()) {
            <tr class="empty-table-row">
              <td
                [attr.colspan]="colSpan()"
                class="empty-table-cell p-8 text-center text-zinc-400 text-sm font-sans"
              >
                {{ displayEmptyMessage() }}
              </td>
            </tr>
          } @else {
            <ng-content select="[table-body]"></ng-content>
            <ng-content></ng-content>
          }
        </tbody>
      </table>
    </div>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class DataTableComponent {
  readonly ariaLabel = input<string | undefined>(undefined);
  readonly isEmpty = input<boolean>(false);
  readonly emptyMessage = input<string | undefined>(undefined);
  readonly colSpan = input<number>(100);

  readonly displayEmptyMessage = computed<string>(() => {
    return this.emptyMessage() || 'No data available';
  });
}
