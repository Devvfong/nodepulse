import {
  RollingAreaChartComponent,
  SparklineComponent,
} from './index';

describe('Shared Charts Barrel Exports', () => {
  it('should export all chart presentation components', () => {
    expect(SparklineComponent).toBeDefined();
    expect(RollingAreaChartComponent).toBeDefined();
  });
});
