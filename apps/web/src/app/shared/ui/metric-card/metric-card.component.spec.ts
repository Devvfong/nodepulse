import { Component } from '@angular/core';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { StatusBadgeComponent } from '../status-badge/status-badge.component';
import { MetricCardComponent } from './metric-card.component';

@Component({
  standalone: true,
  imports: [MetricCardComponent],
  template: `
    <np-metric-card [label]="label" [value]="value" [unit]="unit">
      <div class="custom-chart">Sparkline SVG</div>
    </np-metric-card>
  `,
})
class TestHostComponent {
  label = 'CPU UTILIZATION';
  value: string | number | null = 45.2;
  unit = '%';
}

describe('MetricCardComponent', () => {
  let component: MetricCardComponent;
  let fixture: ComponentFixture<MetricCardComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [MetricCardComponent, StatusBadgeComponent, TestHostComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(MetricCardComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should apply required card container classes', () => {
    fixture.componentRef.setInput('label', 'Memory');
    fixture.detectChanges();
    const cardEl = fixture.nativeElement.firstElementChild as HTMLElement;
    expect(cardEl.className).toContain('bg-zinc-900');
    expect(cardEl.className).toContain('border');
    expect(cardEl.className).toContain('border-zinc-800');
    expect(cardEl.className).toContain('rounded');
    expect(cardEl.className).toContain('p-4');
    expect(cardEl.className).toContain('flex');
    expect(cardEl.className).toContain('flex-col');
    expect(cardEl.className).toContain('justify-between');
  });

  describe('label and subtext', () => {
    it('should render uppercase label with muted typography', () => {
      fixture.componentRef.setInput('label', 'load average');
      fixture.detectChanges();
      const labelEl = fixture.nativeElement.querySelector('.metric-label');
      expect(labelEl).toBeTruthy();
      expect(labelEl?.textContent?.trim()).toBe('load average');
      expect(labelEl?.className).toContain('text-zinc-400');
      expect(labelEl?.className).toContain('text-xs');
      expect(labelEl?.className).toContain('font-medium');
      expect(labelEl?.className).toContain('uppercase');
      expect(labelEl?.className).toContain('tracking-wide');
    });

    it('should render subtext in monospace when provided', () => {
      fixture.componentRef.setInput('label', 'Disk');
      fixture.componentRef.setInput('subtext', 'nvme0n1p2 · 512GB total');
      fixture.detectChanges();
      const subtextEl = fixture.nativeElement.querySelector('.metric-subtext');
      expect(subtextEl).toBeTruthy();
      expect(subtextEl?.textContent?.trim()).toBe('nvme0n1p2 · 512GB total');
      expect(subtextEl?.className).toContain('text-zinc-400');
      expect(subtextEl?.className).toContain('text-xs');
      expect(subtextEl?.className).toContain('font-mono');
    });

    it('should not render subtext element when subtext is omitted', () => {
      fixture.componentRef.setInput('label', 'Disk');
      fixture.detectChanges();
      const subtextEl = fixture.nativeElement.querySelector('.metric-subtext');
      expect(subtextEl).toBeNull();
    });
  });

  describe('value and unit rendering', () => {
    it('should render numeric value with font-mono text-2xl font-bold text-zinc-100', () => {
      fixture.componentRef.setInput('label', 'Tasks');
      fixture.componentRef.setInput('value', 142);
      fixture.detectChanges();
      const valueEl = fixture.nativeElement.querySelector('.metric-value');
      expect(valueEl).toBeTruthy();
      expect(valueEl?.textContent?.trim()).toContain('142');
      expect(valueEl?.className).toContain('font-mono');
      expect(valueEl?.className).toContain('text-2xl');
      expect(valueEl?.className).toContain('font-bold');
      expect(valueEl?.className).toContain('text-zinc-100');
    });

    it('should render " — " fallback when value is null or undefined', () => {
      fixture.componentRef.setInput('label', 'Swap');
      fixture.componentRef.setInput('value', null);
      fixture.detectChanges();
      const valueEl = fixture.nativeElement.querySelector('.metric-value');
      expect(valueEl?.textContent?.trim()).toBe('—');
    });

    it('should render unit next to value when provided', () => {
      fixture.componentRef.setInput('label', 'Memory');
      fixture.componentRef.setInput('value', 32);
      fixture.componentRef.setInput('unit', 'GB');
      fixture.detectChanges();
      const unitEl = fixture.nativeElement.querySelector('.metric-unit');
      expect(unitEl).toBeTruthy();
      expect(unitEl?.textContent?.trim()).toBe('GB');
    });

    it('should uphold neutral percentage rendering invariant (no red/amber threshold styling)', () => {
      // 98% usage must remain neutral text-zinc-100, not artificially styled rose or amber
      fixture.componentRef.setInput('label', 'CPU Usage');
      fixture.componentRef.setInput('value', 98.5);
      fixture.componentRef.setInput('unit', '%');
      fixture.detectChanges();
      const valueEl = fixture.nativeElement.querySelector('.metric-value');
      expect(valueEl?.className).toContain('text-zinc-100');
      expect(valueEl?.className).not.toContain('text-rose-400');
      expect(valueEl?.className).not.toContain('text-amber-400');
      expect(valueEl?.className).not.toContain('text-red-500');
    });
  });

  describe('status badge integration', () => {
    it('should render np-status-badge when status input is provided', () => {
      fixture.componentRef.setInput('label', 'Agent Status');
      fixture.componentRef.setInput('value', 'Online');
      fixture.componentRef.setInput('status', 'healthy');
      fixture.detectChanges();
      const badgeEl = fixture.nativeElement.querySelector('np-status-badge');
      expect(badgeEl).toBeTruthy();
    });

    it('should not render np-status-badge when status is omitted', () => {
      fixture.componentRef.setInput('label', 'Agent Status');
      fixture.componentRef.setInput('value', 'Online');
      fixture.detectChanges();
      const badgeEl = fixture.nativeElement.querySelector('np-status-badge');
      expect(badgeEl).toBeNull();
    });
  });

  describe('content projection', () => {
    it('should project custom content such as charts or details into card', () => {
      const hostFixture = TestBed.createComponent(TestHostComponent);
      hostFixture.detectChanges();
      const projectedEl = hostFixture.nativeElement.querySelector('.custom-chart');
      expect(projectedEl).toBeTruthy();
      expect(projectedEl?.textContent?.trim()).toBe('Sparkline SVG');
    });
  });
});
