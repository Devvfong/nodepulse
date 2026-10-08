import { ComponentFixture, TestBed } from '@angular/core/testing';
import { SparklineComponent } from './sparkline.component';

describe('SparklineComponent', () => {
  let component: SparklineComponent;
  let fixture: ComponentFixture<SparklineComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [SparklineComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(SparklineComponent);
    component = fixture.componentInstance;
  });

  it('should create the sparkline component', () => {
    expect(component).toBeTruthy();
  });

  describe('default inputs', () => {
    it('should have correct default inputs and SVG attributes', () => {
      fixture.detectChanges();
      const svgEl = fixture.nativeElement.querySelector('svg');
      const pathEl = fixture.nativeElement.querySelector('path');

      expect(svgEl).toBeTruthy();
      expect(pathEl).toBeTruthy();
      expect(svgEl.getAttribute('width')).toBe('120');
      expect(svgEl.getAttribute('height')).toBe('32');
      expect(svgEl.getAttribute('viewBox')).toBe('0 0 120 32');
      expect(pathEl.getAttribute('stroke')).toBe('#34d399');
      expect(pathEl.getAttribute('stroke-width')).toBe('1.5');
    });
  });

  describe('test case (a): empty array and all-null arrays', () => {
    it('should render a neutral flat baseline path when data is empty', () => {
      fixture.componentRef.setInput('data', []);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).toBe('M 0 16 L 120 16');
    });

    it('should render a neutral flat baseline path when data contains only nulls', () => {
      fixture.componentRef.setInput('data', [null, null, null, null]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).toBe('M 0 16 L 120 16');
    });
  });

  describe('test case (b): single value', () => {
    it('should render valid coordinates without NaN for a single element array', () => {
      fixture.componentRef.setInput('data', [42]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).not.toContain('Infinity');
      expect(d).toBe('M 0 30');
    });

    it('should render valid coordinates without NaN for a single value with surrounding nulls', () => {
      fixture.componentRef.setInput('data', [null, 42, null]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).not.toContain('Infinity');
      expect(d).toBe('M 60 30');
    });
  });

  describe('test case (c): monotonic data', () => {
    it('should render valid connected path with M and L commands for monotonic data', () => {
      fixture.componentRef.setInput('data', [10, 20, 30]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).toBe('M 0 30 L 60 16 L 120 2');

      // Verify M and L commands exist
      const mMatches = d.match(/M/g);
      const lMatches = d.match(/L/g);
      expect(mMatches?.length).toBe(1);
      expect(lMatches?.length).toBe(2);
    });

    it('should correctly handle constant values without division by zero', () => {
      fixture.componentRef.setInput('data', [50, 50, 50]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).toBe('M 0 30 L 60 30 L 120 30');
    });
  });

  describe('test case (d): gap preservation with null values', () => {
    it('should preserve null values as disconnected path segments containing multiple M commands', () => {
      // 5 items, index 2 is null
      fixture.componentRef.setInput('data', [10, 20, null, 20, 30]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      // Segment 1: M 0 30 L 30 16
      // Gap at index 2 (x=60)
      // Segment 2: M 90 16 L 120 2
      expect(d).toBe('M 0 30 L 30 16 M 90 16 L 120 2');

      const mMatches = d.match(/M/g);
      expect(mMatches?.length).toBe(2);
      const lMatches = d.match(/L/g);
      expect(lMatches?.length).toBe(2);
    });

    it('should handle leading and trailing nulls without broken commands', () => {
      fixture.componentRef.setInput('data', [null, 10, 20, null]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      expect(d).toBe('M 40 30 L 80 2');

      const mMatches = d.match(/M/g);
      expect(mMatches?.length).toBe(1);
    });

    it('should handle multiple disjoint gaps cleanly', () => {
      fixture.componentRef.setInput('data', [10, null, 20, null, 30]);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBeTruthy();
      expect(d).not.toContain('NaN');
      // Each valid sample isolated by nulls forms an M segment
      const mMatches = d.match(/M/g);
      expect(mMatches?.length).toBe(3);
      expect(d).toBe('M 0 30 M 60 16 M 120 2');
    });
  });

  describe('test case (e): viewBox and dimension attributes', () => {
    it('should update viewBox, width, and height when inputs change', () => {
      fixture.componentRef.setInput('width', 240);
      fixture.componentRef.setInput('height', 64);
      fixture.componentRef.setInput('strokeColor', '#38bdf8');
      fixture.componentRef.setInput('strokeWidth', 2);
      fixture.detectChanges();

      const svgEl = fixture.nativeElement.querySelector('svg');
      const pathEl = fixture.nativeElement.querySelector('path');

      expect(svgEl.getAttribute('width')).toBe('240');
      expect(svgEl.getAttribute('height')).toBe('64');
      expect(svgEl.getAttribute('viewBox')).toBe('0 0 240 64');
      expect(pathEl.getAttribute('stroke')).toBe('#38bdf8');
      expect(pathEl.getAttribute('stroke-width')).toBe('2');
    });

    it('should adjust path baseline coordinates when width and height change', () => {
      fixture.componentRef.setInput('width', 200);
      fixture.componentRef.setInput('height', 50);
      fixture.componentRef.setInput('data', []);
      fixture.detectChanges();

      const pathEl = fixture.nativeElement.querySelector('path');
      const d = pathEl.getAttribute('d');

      expect(d).toBe('M 0 25 L 200 25');
    });
  });
});
