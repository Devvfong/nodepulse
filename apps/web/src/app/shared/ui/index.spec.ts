import {
  DataTableComponent,
  DrawerComponent,
  EmptyStateComponent,
  MetricCardComponent,
  StatusBadgeComponent,
} from './index';

describe('Shared UI Barrel Exports', () => {
  it('should export all shared UI presentation primitives including DrawerComponent', () => {
    expect(StatusBadgeComponent).toBeDefined();
    expect(MetricCardComponent).toBeDefined();
    expect(DataTableComponent).toBeDefined();
    expect(EmptyStateComponent).toBeDefined();
    expect(DrawerComponent).toBeDefined();
  });
});
