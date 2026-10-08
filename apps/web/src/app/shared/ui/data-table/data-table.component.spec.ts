import { Component } from '@angular/core';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { DataTableComponent } from './data-table.component';

@Component({
  standalone: true,
  imports: [DataTableComponent],
  template: `
    <np-data-table [isEmpty]="isEmpty" [emptyMessage]="emptyMessage" [colSpan]="3">
      <tr table-header>
        <th scope="col">PID</th>
        <th scope="col">NAME</th>
        <th scope="col">CPU%</th>
      </tr>
      <tr table-body>
        <td>1</td>
        <td>systemd</td>
        <td>0.1%</td>
      </tr>
    </np-data-table>
  `,
})
class TestHostTableComponent {
  isEmpty = false;
  emptyMessage = 'No processes available';
}

describe('DataTableComponent', () => {
  let component: DataTableComponent;
  let fixture: ComponentFixture<DataTableComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [DataTableComponent, TestHostTableComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(DataTableComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should render responsive overflow container with zinc surface styling', () => {
    fixture.detectChanges();
    const container = fixture.nativeElement.firstElementChild as HTMLElement;
    expect(container.className).toContain('overflow-x-auto');
    expect(container.className).toContain('border');
    expect(container.className).toContain('border-zinc-800');
    expect(container.className).toContain('rounded');
    expect(container.className).toContain('bg-zinc-900');
  });

  it('should render accessible table semantic elements (table, thead, tbody)', () => {
    fixture.detectChanges();
    const table = fixture.nativeElement.querySelector('table');
    const thead = fixture.nativeElement.querySelector('thead');
    const tbody = fixture.nativeElement.querySelector('tbody');

    expect(table).toBeTruthy();
    expect(thead).toBeTruthy();
    expect(tbody).toBeTruthy();
  });

  it('should forward optional ariaLabel to table element', () => {
    fixture.componentRef.setInput('ariaLabel', 'Processes Table');
    fixture.detectChanges();
    const table = fixture.nativeElement.querySelector('table');
    expect(table?.getAttribute('aria-label')).toBe('Processes Table');
  });

  describe('content projection and empty state', () => {
    it('should project headers and rows when isEmpty is false', () => {
      const hostFixture = TestBed.createComponent(TestHostTableComponent);
      hostFixture.componentInstance.isEmpty = false;
      hostFixture.detectChanges();

      const hostEl = hostFixture.nativeElement as HTMLElement;
      const ths = hostEl.querySelectorAll('thead th');
      expect(ths.length).toBe(3);
      expect(ths[0].textContent?.trim()).toBe('PID');

      const tds = hostEl.querySelectorAll('tbody td');
      expect(tds.length).toBe(3);
      expect(tds[0].textContent?.trim()).toBe('1');
      expect(tds[1].textContent?.trim()).toBe('systemd');

      const emptyRow = hostEl.querySelector('.empty-table-row');
      expect(emptyRow).toBeNull();
    });

    it('should display emptyMessage row when isEmpty is true', () => {
      const hostFixture = TestBed.createComponent(TestHostTableComponent);
      hostFixture.componentInstance.isEmpty = true;
      hostFixture.componentInstance.emptyMessage = 'No processes available';
      hostFixture.detectChanges();

      const hostEl = hostFixture.nativeElement as HTMLElement;
      const emptyRow = hostEl.querySelector('.empty-table-row');
      expect(emptyRow).toBeTruthy();
      expect(emptyRow?.textContent?.trim()).toContain('No processes available');

      const emptyCell = emptyRow?.querySelector('td');
      expect(emptyCell?.getAttribute('colspan')).toBe('3');
    });

    it('should fall back to default message if emptyMessage is not provided when isEmpty is true', () => {
      fixture.componentRef.setInput('isEmpty', true);
      fixture.detectChanges();

      const emptyCell = fixture.nativeElement.querySelector('.empty-table-cell');
      expect(emptyCell).toBeTruthy();
      expect(emptyCell?.textContent?.trim()).toBe('No data available');
    });
  });
});
