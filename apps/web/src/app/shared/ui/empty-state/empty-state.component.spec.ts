import { Component } from '@angular/core';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { EmptyStateComponent } from './empty-state.component';

@Component({
  standalone: true,
  imports: [EmptyStateComponent],
  template: `
    <np-empty-state
      title="No Containers Found"
      description="Docker daemon returned an empty container list or service is stopped."
    >
      <button class="retry-button">Retry Connection</button>
    </np-empty-state>
  `,
})
class TestHostEmptyStateComponent {}

describe('EmptyStateComponent', () => {
  let component: EmptyStateComponent;
  let fixture: ComponentFixture<EmptyStateComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [EmptyStateComponent, TestHostEmptyStateComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(EmptyStateComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  it('should apply required container styling classes', () => {
    fixture.componentRef.setInput('title', 'Empty View');
    fixture.detectChanges();
    const container = fixture.nativeElement.firstElementChild as HTMLElement;
    expect(container.className).toContain('border');
    expect(container.className).toContain('border-dashed');
    expect(container.className).toContain('border-zinc-800');
    expect(container.className).toContain('rounded');
    expect(container.className).toContain('p-8');
    expect(container.className).toContain('text-center');
    expect(container.className).toContain('bg-zinc-900/40');
    expect(container.className).toContain('flex');
    expect(container.className).toContain('flex-col');
    expect(container.className).toContain('items-center');
    expect(container.className).toContain('justify-center');
  });

  describe('title and description', () => {
    it('should render title with required typography', () => {
      fixture.componentRef.setInput('title', 'No services found');
      fixture.detectChanges();
      const titleEl = fixture.nativeElement.querySelector('.empty-state-title');
      expect(titleEl).toBeTruthy();
      expect(titleEl?.textContent?.trim()).toBe('No services found');
      expect(titleEl?.className).toContain('text-zinc-200');
      expect(titleEl?.className).toContain('text-base');
      expect(titleEl?.className).toContain('font-medium');
    });

    it('should render description when provided', () => {
      fixture.componentRef.setInput('title', 'No services');
      fixture.componentRef.setInput('description', 'Try clearing your search query.');
      fixture.detectChanges();
      const descEl = fixture.nativeElement.querySelector('.empty-state-description');
      expect(descEl).toBeTruthy();
      expect(descEl?.textContent?.trim()).toBe('Try clearing your search query.');
      expect(descEl?.className).toContain('text-zinc-400');
      expect(descEl?.className).toContain('text-sm');
      expect(descEl?.className).toContain('mt-1');
      expect(descEl?.className).toContain('max-w-md');
    });

    it('should not render description element when omitted', () => {
      fixture.componentRef.setInput('title', 'No services');
      fixture.detectChanges();
      const descEl = fixture.nativeElement.querySelector('.empty-state-description');
      expect(descEl).toBeNull();
    });
  });

  describe('icon input', () => {
    it('should render icon element when icon input is provided', () => {
      fixture.componentRef.setInput('title', 'Not Found');
      fixture.componentRef.setInput('icon', 'i-lucide-box');
      fixture.detectChanges();
      const iconEl = fixture.nativeElement.querySelector('.empty-state-icon');
      expect(iconEl).toBeTruthy();
      expect(iconEl?.textContent?.trim() || iconEl?.className).toContain('i-lucide-box');
    });

    it('should not render icon element when icon is omitted', () => {
      fixture.componentRef.setInput('title', 'Not Found');
      fixture.detectChanges();
      const iconEl = fixture.nativeElement.querySelector('.empty-state-icon');
      expect(iconEl).toBeNull();
    });
  });

  describe('content projection', () => {
    it('should project action buttons into component', () => {
      const hostFixture = TestBed.createComponent(TestHostEmptyStateComponent);
      hostFixture.detectChanges();
      const button = hostFixture.nativeElement.querySelector('.retry-button');
      expect(button).toBeTruthy();
      expect(button?.textContent?.trim()).toBe('Retry Connection');
    });
  });
});
