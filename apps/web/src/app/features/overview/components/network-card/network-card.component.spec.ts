import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { By } from '@angular/platform-browser';
import { describe, expect, it, beforeEach } from 'vitest';
import { NetworkCardComponent } from './network-card.component';
import { HostStateService } from '../../../../core/services/host-state.service';
import { SseService } from '../../../../core/api/sse.service';
import { RollingAreaChartComponent } from '../../../../shared/charts/rolling-area-chart/rolling-area-chart.component';
import { StatusBadgeComponent } from '../../../../shared/ui/status-badge/status-badge.component';
import { NetworkInterfaceMetrics } from '../../../../core/models/network.model';
import { MetricPulse, TelemetrySample } from '../../../../core/models/pulse.model';

describe('NetworkCardComponent', () => {
  let fixture: ComponentFixture<NetworkCardComponent>;
  let component: NetworkCardComponent;

  const mockNetworkMetrics = signal<NetworkInterfaceMetrics[] | null>(null);
  const mockNetworkRxRollingBuffer = signal<TelemetrySample<number | null>[]>([]);
  const mockNetworkTxRollingBuffer = signal<TelemetrySample<number | null>[]>([]);
  const mockPulse = signal<MetricPulse | null>(null);

  const mockHostStateService = {
    networkMetrics: mockNetworkMetrics,
    networkRxRollingBuffer: mockNetworkRxRollingBuffer,
    networkTxRollingBuffer: mockNetworkTxRollingBuffer,
  };

  const mockSseService = {
    pulse: mockPulse,
  };

  beforeEach(async () => {
    mockNetworkMetrics.set(null);
    mockNetworkRxRollingBuffer.set([]);
    mockNetworkTxRollingBuffer.set([]);
    mockPulse.set(null);

    await TestBed.configureTestingModule({
      imports: [NetworkCardComponent],
      providers: [
        { provide: HostStateService, useValue: mockHostStateService },
        { provide: SseService, useValue: mockSseService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(NetworkCardComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should render fallback throughput dashes when pulse is null', () => {
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('NETWORK THROUGHPUT');
    expect(el.textContent).toContain('—');
  });

  it('should render headline throughput rates from SSE pulse', () => {
    mockPulse.set({
      timestamp: '2026-10-08T12:00:00Z',
      cpu_usage_percent: 10,
      memory_usage_percent: 40,
      memory_used_bytes: 4000000000,
      network_rx_bytes_sec: 1048576 * 5.2, // 5.2 MB/s
      network_tx_bytes_sec: 1048576 * 1.8, // 1.8 MB/s
    });

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;

    expect(el.textContent).toContain('5.2 MB/s');
    expect(el.textContent).toContain('1.8 MB/s');
  });

  it('should render interface details and hardware error tallies without packet drops', () => {
    mockNetworkMetrics.set([
      {
        name: 'eth0',
        mac_address: '00:11:22:33:44:55',
        operstate: 'up',
        speed_mbps: 1000,
        rx_bytes: 1000000,
        tx_bytes: 500000,
        rx_packets: 1000,
        tx_packets: 500,
        rx_errors: 0,
        tx_errors: 2,
        rx_bytes_per_sec: 1024,
        tx_bytes_per_sec: 512,
      },
    ]);

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;

    expect(el.textContent).toContain('eth0');
    expect(el.textContent).toContain('1000 Mbps');
    expect(el.textContent).toContain('Errors: 0 RX / 2 TX');

    // Strict invariant: ZERO drop counters in text!
    expect(el.textContent?.toLowerCase()).not.toContain('drop');

    const badge = fixture.debugElement.query(By.directive(StatusBadgeComponent));
    expect(badge).toBeTruthy();
    expect((badge.componentInstance as StatusBadgeComponent).status()).toBe('up');
  });

  it('should render dual-trace RollingAreaChartComponent bound to RX and TX buffers', () => {
    const rxSamples: TelemetrySample<number | null>[] = [{ timestamp: '2026-10-08T12:00:00Z', value: 1000 }];
    const txSamples: TelemetrySample<number | null>[] = [{ timestamp: '2026-10-08T12:00:00Z', value: 500 }];

    mockNetworkRxRollingBuffer.set(rxSamples);
    mockNetworkTxRollingBuffer.set(txSamples);

    fixture.detectChanges();
    const chart = fixture.debugElement.query(By.directive(RollingAreaChartComponent));
    expect(chart).toBeTruthy();

    const chartComp = chart.componentInstance as RollingAreaChartComponent;
    expect(chartComp.samples()).toEqual(rxSamples);
    expect(chartComp.secondarySamples()).toEqual(txSamples);
    expect(chartComp.primaryLabel()).toBe('RX');
    expect(chartComp.secondaryLabel()).toBe('TX');
  });
});
