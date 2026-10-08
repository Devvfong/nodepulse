import { ChangeDetectionStrategy, Component, computed, input } from '@angular/core';

export type BadgeTone = 'emerald' | 'amber' | 'rose' | 'zinc';

const EMERALD_STATUSES = new Set(['healthy', 'active', 'running', 'ready', 'connected', 'up', 'r']);
const AMBER_STATUSES = new Set(['warning', 'warming_up', 'reconnecting', 'cached']);
const ROSE_STATUSES = new Set(['critical', 'failed', 'unreachable', 'error', 'down', 'z']);

@Component({
  selector: 'np-status-badge',
  standalone: true,
  template: `
    <span [class]="badgeClass()" role="status">
      <span class="status-dot w-1.5 h-1.5 rounded-full shrink-0" [class]="dotClass()" aria-hidden="true"></span>
      <span class="status-label">{{ displayLabel() }}</span>
    </span>
  `,
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class StatusBadgeComponent {
  readonly status = input<string>('unknown');
  readonly label = input<string | undefined>(undefined);

  readonly tone = computed<BadgeTone>(() => {
    const raw = (this.status() || '').toLowerCase().trim().replace(/[-_]/g, '_');
    if (EMERALD_STATUSES.has(raw)) {
      return 'emerald';
    }
    if (AMBER_STATUSES.has(raw)) {
      return 'amber';
    }
    if (ROSE_STATUSES.has(raw)) {
      return 'rose';
    }
    return 'zinc';
  });

  readonly displayLabel = computed<string>(() => {
    const customLabel = this.label();
    if (customLabel !== undefined && customLabel !== null && customLabel !== '') {
      return customLabel;
    }
    const raw = this.status() || 'unknown';
    return raw.trim().replace(/[-_]/g, ' ').toUpperCase();
  });

  readonly badgeClass = computed<string>(() => {
    const base = 'font-mono text-xs px-2 py-0.5 rounded-sm inline-flex items-center gap-1.5 tracking-wider font-semibold border';
    switch (this.tone()) {
      case 'emerald':
        return `${base} text-emerald-400 bg-emerald-950/40 border-emerald-800/50`;
      case 'amber':
        return `${base} text-amber-400 bg-amber-950/40 border-amber-800/50`;
      case 'rose':
        return `${base} text-rose-400 bg-rose-950/40 border-rose-800/50`;
      case 'zinc':
      default:
        return `${base} text-zinc-400 bg-zinc-800/60 border-zinc-700/50`;
    }
  });

  readonly dotClass = computed<string>(() => {
    switch (this.tone()) {
      case 'emerald':
        return 'bg-emerald-400';
      case 'amber':
        return 'bg-amber-400';
      case 'rose':
        return 'bg-rose-400';
      case 'zinc':
      default:
        return 'bg-zinc-400';
    }
  });
}
