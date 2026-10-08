import { HeaderComponent, ShellComponent, SidebarComponent } from './index';

describe('Layout Barrel Exports', () => {
  it('should export all layout components', () => {
    expect(SidebarComponent).toBeDefined();
    expect(HeaderComponent).toBeDefined();
    expect(ShellComponent).toBeDefined();
  });
});
