import { Routes } from '@angular/router';

export const routes: Routes = [
  {
    path: '',
    pathMatch: 'full',
    redirectTo: 'overview',
  },
  {
    path: 'overview',
    loadComponent: () =>
      import('./features/overview/overview.component').then((m) => m.OverviewComponent),
  },
  {
    path: 'processes',
    loadComponent: () =>
      import('./features/processes/processes.component').then((m) => m.ProcessesComponent),
  },
  {
    path: 'services',
    loadComponent: () =>
      import('./features/services/services.component').then((m) => m.ServicesComponent),
  },
  {
    path: 'containers',
    loadComponent: () =>
      import('./features/containers/containers.component').then((m) => m.ContainersComponent),
  },
  {
    path: '**',
    redirectTo: 'overview',
  },
];
