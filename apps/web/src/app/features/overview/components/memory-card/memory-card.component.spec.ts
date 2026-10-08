import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { By } from '@angular/platform-browser';
import { describe, expect, it, beforeEach } from 'vitest';
import { MemoryCardComponent } from './memory-card.component';
import { HostStateService } from '../../../../core/services/host-state.service';
import { RollingAreaChartComponent } from '../../../../shared/charts/rolling-area-chart/rolling-area-chart.component';
import { MemoryMetrics } from '../../../../core/models/memory.model';
import { TelemetrySample } from '../../../../core/models/pulse.model';

describe('MemoryCardComponent', () => {
  let fixture: ComponentFixture<MemoryCardComponent>;
  let component: MemoryCardComponent;

  const mockMemoryMetrics = signal<MemoryMetrics | null>(null);
  const mockMemoryRollingBuffer = signal<TelemetrySample<number | null>[]>([]);

  const mockHostStateService = {
    memoryMetrics: mockMemoryMetrics,
    memoryRollingBuffer: mockMemoryRollingBuffer,
  };

  beforeEach(async () => {
    mockMemoryMetrics.set(null);
    mockMemoryRollingBuffer.set([]);

    await TestBed.configureTestingModule({
      imports: [MemoryCardComponent],
      providers: [
        { provide: HostStateService, useValue: mockHostStateService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(MemoryCardComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should render fallback dashes when memoryMetrics is null', () => {
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('MEMORY USAGE');
    expect(el.textContent).toContain('—');
  });

  it('should render memory values formatted cleanly when data is available', () => {
    mockMemoryMetrics.set({
      total_bytes: 16 * 1024 * 1024 * 1024,
      used_bytes: 8 * 1024 * 1024 * 1024,
      free_bytes: 4 * 1024 * 1024 * 1024,
      available_bytes: 8 * 1024 * 1024 * 1024,
      buffers_bytes: 512 * 1024 * 1024,
      cached_bytes: 3584 * 1024 * 1024,
      usage_percent: 50.0,
      swap_total_bytes: 4 * 1024 * 1024 * 1024,
      swap_free_bytes: 3 * 1024 * 1024 * 1024,
      swap_used_bytes: 1 * 1024 * 1024 * 1024,
      swap_usage_percent: 25.0,
    });

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;

    expect(el.textContent).toContain('50.0%');
    expect(el.textContent).toContain('8.0 GB / 16.0 GB');
    expect(el.textContent).toContain('512.0 MB');
    expect(el.textContent).toContain('3.5 GB');
    expect(el.textContent).toContain('25.0%');
    expect(el.textContent).toContain('1.0 GB / 4.0 GB');
  });

  it('should render RollingAreaChartComponent bound to memoryRollingBuffer', () => {
    const samples: TelemetrySample<number | null>[] = [
      { timestamp: '2026-10-08T12:00:00Z', value: 48 },
      { timestamp: '2026-10-08T12:00:01Z', value: 50 },
    ];
    mockMemoryRollingBuffer.set(samples);

    fixture.detectChanges();
    const chart = fixture.debugElement.query(By.directive(RollingAreaChartComponent));
    expect(chart).toBeTruthy();

    const chartComp = chart.componentInstance as RollingAreaChartComponent;
    expect(chartComp.samples()).toEqual(samples);
    expect(chartComp.maxValue()).toBe(100);
  });
});
