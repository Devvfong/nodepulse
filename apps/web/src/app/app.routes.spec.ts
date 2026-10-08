import { TestBed } from '@angular/core/testing';
import { Router, provideRouter } from '@angular/router';
import { routes } from './app.routes';
import { OverviewComponent } from './features/overview/overview.component';
import { ProcessesComponent } from './features/processes/processes.component';
import { ServicesComponent } from './features/services/services.component';
import { ContainersComponent } from './features/containers/containers.component';

describe('AppRoutes', () => {
  let router: Router;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      providers: [provideRouter(routes)],
    }).compileComponents();

    router = TestBed.inject(Router);
  });

  it('should redirect empty path to /overview', async () => {
    await router.navigateByUrl('');
    expect(router.url).toBe('/overview');
  });

  it('should navigate to /overview and resolve OverviewComponent', async () => {
    const success = await router.navigateByUrl('/overview');
    expect(success).toBe(true);
    expect(router.url).toBe('/overview');
  });

  it('should navigate to /processes and resolve ProcessesComponent', async () => {
    const success = await router.navigateByUrl('/processes');
    expect(success).toBe(true);
    expect(router.url).toBe('/processes');
  });

  it('should navigate to /services and resolve ServicesComponent', async () => {
    const success = await router.navigateByUrl('/services');
    expect(success).toBe(true);
    expect(router.url).toBe('/services');
  });

  it('should navigate to /containers and resolve ContainersComponent', async () => {
    const success = await router.navigateByUrl('/containers');
    expect(success).toBe(true);
    expect(router.url).toBe('/containers');
  });

  it('should redirect unknown wildcard path to /overview', async () => {
    await router.navigateByUrl('/unknown-route-path');
    expect(router.url).toBe('/overview');
  });

  it('should have lazy loadComponent functions defined on feature routes', async () => {
    const overviewRoute = routes.find((r) => r.path === 'overview');
    const processesRoute = routes.find((r) => r.path === 'processes');
    const servicesRoute = routes.find((r) => r.path === 'services');
    const containersRoute = routes.find((r) => r.path === 'containers');

    expect(typeof overviewRoute?.loadComponent).toBe('function');
    expect(typeof processesRoute?.loadComponent).toBe('function');
    expect(typeof servicesRoute?.loadComponent).toBe('function');
    expect(typeof containersRoute?.loadComponent).toBe('function');

    const loadedOverview = await (overviewRoute?.loadComponent as () => Promise<any>)();
    const loadedProcesses = await (processesRoute?.loadComponent as () => Promise<any>)();
    const loadedServices = await (servicesRoute?.loadComponent as () => Promise<any>)();
    const loadedContainers = await (containersRoute?.loadComponent as () => Promise<any>)();

    expect(loadedOverview).toBe(OverviewComponent);
    expect(loadedProcesses).toBe(ProcessesComponent);
    expect(loadedServices).toBe(ServicesComponent);
    expect(loadedContainers).toBe(ContainersComponent);
  });
});
