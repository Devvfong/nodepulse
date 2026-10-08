import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { By } from '@angular/platform-browser';
import { describe, expect, it, beforeEach } from 'vitest';
import { CpuCardComponent } from './cpu-card.component';
import { HostStateService } from '../../../../core/services/host-state.service';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { RollingAreaChartComponent } from '../../../../shared/charts/rolling-area-chart/rolling-area-chart.component';
import { CpuMetrics } from '../../../../core/models/cpu.model';
import { TelemetrySample } from '../../../../core/models/pulse.model';

describe('CpuCardComponent', () => {
  let fixture: ComponentFixture<CpuCardComponent>;
  let component: CpuCardComponent;

  const mockCpuMetrics = signal<CpuMetrics | null>(null);
  const mockCpuRollingBuffer = signal<TelemetrySample<number | null>[]>([]);

  const mockHostStateService = {
    cpuMetrics: mockCpuMetrics,
    cpuRollingBuffer: mockCpuRollingBuffer,
  };

  beforeEach(async () => {
    mockCpuMetrics.set(null);
    mockCpuRollingBuffer.set([]);

    await TestBed.configureTestingModule({
      imports: [CpuCardComponent],
      providers: [
        { provide: HostStateService, useValue: mockHostStateService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(CpuCardComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should render fallback placeholder when cpuMetrics is null', () => {
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('CPU USAGE');
    expect(el.textContent).toContain('—');
  });

  it('should render warming_up badge when usage_percent is null and measurement_status is warming_up', () => {
    mockCpuMetrics.set({
      usage_percent: null,
      measurement_status: 'warming_up',
      model_name: 'AMD EPYC 7763',
      physical_cores: 4,
      logical_cores: 8,
      load_average: { one_minute: 0.1, five_minute: 0.2, fifteen_minute: 0.3 },
      cores: [],
    });

    fixture.detectChanges();
    const badge = fixture.debugElement.query(By.directive(StatusBadgeComponent));
    expect(badge).toBeTruthy();

    const badgeComp = badge.componentInstance as StatusBadgeComponent;
    expect(badgeComp.status()).toBe('warming_up');
    expect(badgeComp.label()).toBe('WARMING UP');

    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('—');
  });

  it('should render ready status badge and metric details when ready', () => {
    mockCpuMetrics.set({
      usage_percent: 24.5,
      measurement_status: 'ready',
      model_name: 'Intel Core i9-13900K',
      physical_cores: 8,
      logical_cores: 16,
      load_average: { one_minute: 1.25, five_minute: 1.10, fifteen_minute: 0.95 },
      cores: [],
    });

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('24.5%');
    expect(el.textContent).toContain('Intel Core i9-13900K');
    expect(el.textContent).toContain('8 physical / 16 logical');
    expect(el.textContent).toContain('1.25');
    expect(el.textContent).toContain('1.10');
    expect(el.textContent).toContain('0.95');
  });

  it('should render RollingAreaChartComponent bound to cpuRollingBuffer', () => {
    const samples: TelemetrySample<number | null>[] = [
      { timestamp: '2026-10-08T12:00:00Z', value: 20 },
      { timestamp: '2026-10-08T12:00:01Z', value: 25 },
    ];
    mockCpuRollingBuffer.set(samples);

    fixture.detectChanges();
    const chart = fixture.debugElement.query(By.directive(RollingAreaChartComponent));
    expect(chart).toBeTruthy();

    const chartComp = chart.componentInstance as RollingAreaChartComponent;
    expect(chartComp.samples()).toEqual(samples);
    expect(chartComp.maxValue()).toBe(100);
  });
});
