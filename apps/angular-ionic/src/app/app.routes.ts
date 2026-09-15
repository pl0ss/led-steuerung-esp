import { Routes } from '@angular/router';
import { TabsPage } from './tabs/tabs.page';

export const routes: Routes = [
  {
    path: '',
    component: TabsPage,
    children: [
      {
        path: 'control',
        loadComponent: () =>
          import('./pages/control/control.page').then((m) => m.ControlPage),
      },
      // {
      //   path: 'placeholder',
      //   loadComponent: () =>
      //     import('./pages/placeholder/placeholder.page').then(
      //       (m) => m.PlaceholderPage,
      //     ),
      // },
      {
        path: '',
        redirectTo: '/control',
        pathMatch: 'full',
      },
    ],
  },
  {
    path: '',
    redirectTo: '/learning',
    pathMatch: 'full',
  },
];
