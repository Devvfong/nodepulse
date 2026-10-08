import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { describe, expect, it, beforeEach } from 'vitest';
import { DisksCardComponent } from './disks-card.component';
import { HostStateService } from '../../../../core/services/host-state.service';
import { DiskPartitionMetrics } from '../../../../core/models/disk.model';

describe('DisksCardComponent', () => {
  let fixture: ComponentFixture<DisksCardComponent>;
  let component: DisksCardComponent;

  const mockDiskMetrics = signal<DiskPartitionMetrics[] | null>(null);

  const mockHostStateService = {
    diskMetrics: mockDiskMetrics,
  };

  beforeEach(async () => {
    mockDiskMetrics.set(null);

    await TestBed.configureTestingModule({
      imports: [DisksCardComponent],
      providers: [
        { provide: HostStateService, useValue: mockHostStateService },
      ],
    }).compileComponents();

    fixture = TestBed.createComponent(DisksCardComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should render empty state message when diskMetrics is null or empty', () => {
    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.textContent).toContain('FILESYSTEM CAPACITY');
    expect(el.textContent).toContain('No filesystem telemetry');
  });

  it('should render partition details, usage percentages, and progress bars', () => {
    mockDiskMetrics.set([
      {
        filesystem: '/dev/nvme0n1p2',
        mount_point: '/',
        fstype: 'ext4',
        total_bytes: 500 * 1024 * 1024 * 1024,
        used_bytes: 200 * 1024 * 1024 * 1024,
        free_bytes: 300 * 1024 * 1024 * 1024,
        available_bytes: 280 * 1024 * 1024 * 1024,
        usage_percent: 40.0,
        inodes_total: 32000000,
        inodes_free: 30000000,
      },
      {
        filesystem: '/dev/nvme0n1p1',
        mount_point: '/boot/efi',
        fstype: 'vfat',
        total_bytes: 512 * 1024 * 1024,
        used_bytes: 64 * 1024 * 1024,
        free_bytes: 448 * 1024 * 1024,
        available_bytes: 448 * 1024 * 1024,
        usage_percent: 12.5,
        inodes_total: 0,
        inodes_free: 0,
      },
    ]);

    fixture.detectChanges();
    const el = fixture.nativeElement as HTMLElement;

    expect(el.textContent).toContain('/');
    expect(el.textContent).toContain('ext4');
    expect(el.textContent).toContain('/dev/nvme0n1p2');
    expect(el.textContent).toContain('40.0%');
    expect(el.textContent).toContain('200.0 GB / 500.0 GB');

    expect(el.textContent).toContain('/boot/efi');
    expect(el.textContent).toContain('12.5%');
  });
});
