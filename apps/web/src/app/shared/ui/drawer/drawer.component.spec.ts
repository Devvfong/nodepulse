import { Component } from '@angular/core';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { DrawerComponent } from './drawer.component';

@Component({
  standalone: true,
  imports: [DrawerComponent],
  template: `
    <np-drawer
      [isOpen]="isOpen"
      [title]="title"
      [subtitle]="subtitle"
      (close)="onClose()"
    >
      <div drawer-header class="custom-header-action">
        <button type="button" class="action-btn">Action</button>
      </div>
      <div class="projected-body-content">
        <p>Drawer Body Content</p>
      </div>
    </np-drawer>
  `,
})
class TestHostDrawerComponent {
  isOpen = true;
  title = 'Process Inspection';
  subtitle: string | undefined = 'PID: 1248';
  closeEmitted = false;

  onClose(): void {
    this.closeEmitted = true;
  }
}

describe('DrawerComponent', () => {
  let component: DrawerComponent;
  let fixture: ComponentFixture<DrawerComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [DrawerComponent, TestHostDrawerComponent],
    }).compileComponents();

    fixture = TestBed.createComponent(DrawerComponent);
    component = fixture.componentInstance;
  });

  it('should create the component', () => {
    expect(component).toBeTruthy();
  });

  describe('a. When isOpen is false', () => {
    it('should not render drawer DOM when isOpen is false', () => {
      fixture.componentRef.setInput('isOpen', false);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.detectChanges();

      const backdrop = fixture.nativeElement.querySelector('.drawer-backdrop');
      const panel = fixture.nativeElement.querySelector('.drawer-panel');
      const dialog = fixture.nativeElement.querySelector('[role="dialog"]');

      expect(backdrop).toBeNull();
      expect(panel).toBeNull();
      expect(dialog).toBeNull();
    });
  });

  describe('b. When isOpen is true', () => {
    it('should render dialog with backdrop, panel, and title', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Inspect Unit: nodepulse.service');
      fixture.detectChanges();

      const backdrop = fixture.nativeElement.querySelector('.drawer-backdrop');
      const panel = fixture.nativeElement.querySelector('.drawer-panel');
      const titleEl = fixture.nativeElement.querySelector('.drawer-title');

      expect(backdrop).toBeTruthy();
      expect(panel).toBeTruthy();
      expect(titleEl).toBeTruthy();
      expect(titleEl?.textContent?.trim()).toBe('Inspect Unit: nodepulse.service');
    });

    it('should apply required geometry and styling classes', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Styling Test');
      fixture.detectChanges();

      const backdrop = fixture.nativeElement.querySelector('.drawer-backdrop');
      expect(backdrop.className).toContain('fixed');
      expect(backdrop.className).toContain('inset-0');
      expect(backdrop.className).toContain('bg-black/60');
      expect(backdrop.className).toContain('z-40');
      expect(backdrop.className).toContain('transition-opacity');

      const panel = fixture.nativeElement.querySelector('.drawer-panel');
      expect(panel.className).toContain('fixed');
      expect(panel.className).toContain('top-0');
      expect(panel.className).toContain('right-0');
      expect(panel.className).toContain('bottom-0');
      expect(panel.className).toContain('h-full');
      expect(panel.className).toContain('w-full');
      expect(panel.className).toContain('max-w-full');
      expect(panel.className).toContain('md:max-w-xl');
      expect(panel.className).toContain('lg:max-w-[480px]');
      expect(panel.className).toContain('bg-zinc-900');
      expect(panel.className).toContain('border-l');
      expect(panel.className).toContain('border-zinc-800');
      expect(panel.className).toContain('z-50');
      expect(panel.className).toContain('flex');
      expect(panel.className).toContain('flex-col');
      expect(panel.className).toContain('shadow-2xl');

      const titleEl = fixture.nativeElement.querySelector('.drawer-title');
      expect(titleEl.className).toContain('text-zinc-100');
      expect(titleEl.className).toContain('font-bold');
      expect(titleEl.className).toContain('text-base');

      const bodyEl = fixture.nativeElement.querySelector('.drawer-body');
      expect(bodyEl.className).toContain('flex-1');
      expect(bodyEl.className).toContain('overflow-y-auto');
      expect(bodyEl.className).toContain('p-4');
      expect(bodyEl.className).toContain('sm:p-6');
      expect(bodyEl.className).toContain('text-zinc-100');
      expect(bodyEl.className).toContain('space-y-4');
    });

    it('should set accessible dialog attributes', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Container Detail');
      fixture.detectChanges();

      const panel = fixture.nativeElement.querySelector('.drawer-panel');
      expect(panel.getAttribute('role')).toBe('dialog');
      expect(panel.getAttribute('aria-modal')).toBe('true');
      expect(panel.getAttribute('aria-label')).toBe('Container Detail');
      expect(panel.getAttribute('tabindex')).toBe('-1');
    });

    it('should render subtitle when provided, and omit when undefined', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.componentRef.setInput('subtitle', 'PID: 1248');
      fixture.detectChanges();

      let subtitleEl = fixture.nativeElement.querySelector('.drawer-subtitle');
      expect(subtitleEl).toBeTruthy();
      expect(subtitleEl?.textContent?.trim()).toBe('PID: 1248');

      fixture.componentRef.setInput('subtitle', undefined);
      fixture.detectChanges();

      subtitleEl = fixture.nativeElement.querySelector('.drawer-subtitle');
      expect(subtitleEl).toBeNull();
    });
  });

  describe('c. Clicking close button', () => {
    it('should emit close output when close button is clicked', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.detectChanges();

      let emitted = false;
      component.close.subscribe(() => {
        emitted = true;
      });

      const closeBtn = fixture.nativeElement.querySelector('button[aria-label="Close drawer"]') as HTMLButtonElement;
      expect(closeBtn).toBeTruthy();
      closeBtn.click();

      expect(emitted).toBe(true);
    });
  });

  describe('d. Clicking backdrop', () => {
    it('should emit close output when backdrop is clicked', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.detectChanges();

      let emitted = false;
      component.close.subscribe(() => {
        emitted = true;
      });

      const backdrop = fixture.nativeElement.querySelector('.drawer-backdrop') as HTMLElement;
      expect(backdrop).toBeTruthy();
      backdrop.click();

      expect(emitted).toBe(true);
    });
  });

  describe('e. Pressing Escape key', () => {
    it('should emit close output when Escape key is pressed while open', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.detectChanges();

      let emitted = false;
      component.close.subscribe(() => {
        emitted = true;
      });

      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));

      expect(emitted).toBe(true);
    });

    it('should not emit close output when Escape key is pressed while closed', () => {
      fixture.componentRef.setInput('isOpen', false);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.detectChanges();

      let emitted = false;
      component.close.subscribe(() => {
        emitted = true;
      });

      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));

      expect(emitted).toBe(false);
    });
  });

  describe('f. Clicking inside drawer panel', () => {
    it('should not emit close output when clicking inside drawer panel or body', () => {
      fixture.componentRef.setInput('isOpen', true);
      fixture.componentRef.setInput('title', 'Process Detail');
      fixture.detectChanges();

      let emitted = false;
      component.close.subscribe(() => {
        emitted = true;
      });

      const panel = fixture.nativeElement.querySelector('.drawer-panel') as HTMLElement;
      const body = fixture.nativeElement.querySelector('.drawer-body') as HTMLElement;

      panel.click();
      expect(emitted).toBe(false);

      body.click();
      expect(emitted).toBe(false);
    });
  });

  describe('g. Content projection', () => {
    it('should project content inside drawer body and drawer-header slot', () => {
      const hostFixture = TestBed.createComponent(TestHostDrawerComponent);
      hostFixture.detectChanges();

      const projectedBody = hostFixture.nativeElement.querySelector('.projected-body-content');
      expect(projectedBody).toBeTruthy();
      expect(projectedBody?.textContent?.trim()).toBe('Drawer Body Content');

      const projectedHeader = hostFixture.nativeElement.querySelector('.custom-header-action');
      expect(projectedHeader).toBeTruthy();
      expect(projectedHeader?.querySelector('.action-btn')).toBeTruthy();

      const hostComponent = hostFixture.componentInstance;
      const closeBtn = hostFixture.nativeElement.querySelector('button[aria-label="Close drawer"]') as HTMLButtonElement;
      closeBtn.click();
      expect(hostComponent.closeEmitted).toBe(true);
    });
  });

  describe('h. Focus management and trapping', () => {
    it('should trap Tab key navigation inside drawer', () => {
      const hostFixture = TestBed.createComponent(TestHostDrawerComponent);
      hostFixture.detectChanges();

      const drawer = hostFixture.nativeElement.querySelector('.drawer-panel') as HTMLElement;
      expect(drawer).toBeTruthy();

      const actionBtn = hostFixture.nativeElement.querySelector('.action-btn') as HTMLButtonElement;
      const closeBtn = hostFixture.nativeElement.querySelector('.drawer-close-btn') as HTMLButtonElement;
      expect(actionBtn).toBeTruthy();
      expect(closeBtn).toBeTruthy();

      // Focus close button (last focusable in header)
      closeBtn.focus();
      expect(document.activeElement).toBe(closeBtn);

      // Press Tab on last focusable element
      const tabEvent = new KeyboardEvent('keydown', { key: 'Tab', cancelable: true });
      const preventDefaultSpy = vi.spyOn(tabEvent, 'preventDefault');
      window.dispatchEvent(tabEvent);

      expect(preventDefaultSpy).toHaveBeenCalled();
      expect(document.activeElement).toBe(actionBtn);
    });

    it('should trap Shift+Tab key navigation to wrap to last element', () => {
      const hostFixture = TestBed.createComponent(TestHostDrawerComponent);
      hostFixture.detectChanges();

      const actionBtn = hostFixture.nativeElement.querySelector('.action-btn') as HTMLButtonElement;
      const closeBtn = hostFixture.nativeElement.querySelector('.drawer-close-btn') as HTMLButtonElement;

      // Focus first focusable
      actionBtn.focus();
      expect(document.activeElement).toBe(actionBtn);

      // Press Shift+Tab on first element
      const shiftTabEvent = new KeyboardEvent('keydown', { key: 'Tab', shiftKey: true, cancelable: true });
      const preventDefaultSpy = vi.spyOn(shiftTabEvent, 'preventDefault');
      window.dispatchEvent(shiftTabEvent);

      expect(preventDefaultSpy).toHaveBeenCalled();
      expect(document.activeElement).toBe(closeBtn);
    });
  });
});
